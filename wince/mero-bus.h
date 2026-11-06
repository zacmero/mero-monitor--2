/* RAM-only, mutex protected mailbox shared by the agent and companion apps.
 * No SD writes for telemetry. One outstanding button press avoids replay bursts. */
#ifndef MERO_BUS_H
#define MERO_BUS_H
#define MERO_BUS_BYTES 4096
typedef struct {
    DWORD tick, generation, epoch;
    char lines[MERO_BUS_BYTES];
    char event[512], eventId[80], ack[80], result[96];
} MeroBus;
static HANDLE mero_bus_map, mero_bus_mutex;
static MeroBus *mero_bus;
static int MeroBusOpen(void) {
    if (mero_bus) return 1;
    mero_bus_mutex = CreateMutexW(NULL, FALSE, L"MERO_COMPANION_BUS_MUTEX_V1");
    mero_bus_map = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                    0, sizeof(MeroBus), L"MERO_COMPANION_BUS_V1");
    if (mero_bus_map) mero_bus = (MeroBus *)MapViewOfFile(mero_bus_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MeroBus));
    return mero_bus && mero_bus_mutex;
}
static int MeroBusLock(void) {
    DWORD result;
    if (!MeroBusOpen()) return 0;
    result = WaitForSingleObject(mero_bus_mutex, 0);
    return result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
}
static void MeroBusClose(void) {
    if (mero_bus) UnmapViewOfFile(mero_bus);
    if (mero_bus_map) CloseHandle(mero_bus_map);
    if (mero_bus_mutex) CloseHandle(mero_bus_mutex);
    mero_bus = NULL; mero_bus_map = mero_bus_mutex = NULL;
}
static int MeroBusCommand(const char *action, const char *track, unsigned int value) {
    int ok = 0;
    DWORD tick = GetTickCount();
    if (!MeroBusLock()) return 0;
    if (mero_bus->generation && (DWORD)(tick - mero_bus->tick) < 15000 &&
        (!mero_bus->event[0] || !strcmp(mero_bus->eventId, mero_bus->ack))) {
        unsigned int epoch = mero_bus->epoch + (DWORD)(tick - mero_bus->tick) / 1000;
        snprintf(mero_bus->eventId, sizeof(mero_bus->eventId), "%u-%lu-%lu", epoch, GetCurrentProcessId(), tick);
        snprintf(mero_bus->event, sizeof(mero_bus->event),
                 "id=%s\ncreated=%u\naction=%s\ntrack=%s\nvalue=%u\n",
                 mero_bus->eventId, epoch, action, track, value);
        ok = 1;
    }
    ReleaseMutex(mero_bus_mutex);
    return ok;
}
#endif
