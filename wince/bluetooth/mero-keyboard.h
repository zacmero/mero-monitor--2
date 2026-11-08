/* Explicit, signed PC keyboard requests. No OEM/system window targets.
 * Journal before delivery: uncertain interrupted input must never replay. */
#ifndef MERO_KEYBOARD_H
#define MERO_KEYBOARD_H
static int MeroKeyboardTarget(const char *name) {
    return !strcmp(name,"MERO_TERMINAL")||!strcmp(name,"MeroShellWndClass")||
        !strcmp(name,"MERO_AMBIENT")||!strcmp(name,"MERO_BACKLIGHT")||
        !strcmp(name,"MERO_MUSIC");
}
static unsigned int MeroKeyboardEscape(const unsigned char *p,unsigned int n,unsigned int *used) {
    *used=0;
    if(n>=3&&p[0]==27&&p[1]=='['){
        *used=3;
        switch(p[2]){case 'A':return VK_UP;case 'B':return VK_DOWN;case 'C':return VK_RIGHT;
            case 'D':return VK_LEFT;case 'H':return VK_HOME;case 'F':return VK_END;}
        if(n>=4&&p[3]=='~'){*used=4;switch(p[2]){case '3':return VK_DELETE;case '5':return VK_PRIOR;case '6':return VK_NEXT;}}
    }
    *used=0;return 0;
}
static int MeroKeyboardTextValid(const unsigned char *p,unsigned int n) {
    unsigned int i=0;
    while(i<n){unsigned int c=p[i++];
        if(c<128)continue;
        if(c>=0xc2&&c<=0xdf){if(i>=n||(p[i]&0xc0)!=0x80)return 0;i++;}
        else if(c>=0xe0&&c<=0xef){
            if(i+1>=n||(p[i]&0xc0)!=0x80||(p[i+1]&0xc0)!=0x80||
               (c==0xe0&&p[i]<0xa0)||(c==0xed&&p[i]>=0xa0))return 0;i+=2;
        }else return 0; /* CE WCHAR: no supplementary-plane keys. */
    }return 1;
}
static void AgentKeyboard(const char *field) {
    char copy[192],*target,*hex,id[17],cached[512],oldId[32],result[512];
    WCHAR wide[64],actual[64];unsigned char bytes[32];unsigned int i,len;
    HWND foreground;const char *outcome="wrong_foreground_app";
    if(!field[0]||strlen(field)>=sizeof(copy))return;
    strcpy(copy,field);target=strchr(copy,':');if(!target)return;*target++=0;
    hex=strchr(target,':');if(!hex)return;*hex++=0;
    if(strlen(copy)!=16||!AuthUnhex(copy,bytes,8)||(!MeroKeyboardTarget(target)&&strcmp(target,"MERO_FOREGROUND")))return;
    strcpy(id,copy);len=strlen(hex)/2;
    if(!len||len>32||strlen(hex)!=len*2||!AuthUnhex(hex,bytes,len)||!MeroKeyboardTextValid(bytes,len))return;
    i=AgentRead(L"\\Temp\\MeroKeyboardResult.txt",cached,sizeof(cached));
    if(i&&AgentVerify(cached,i,agentKey)&&AgentGet(cached,"id",oldId,sizeof(oldId))&&!strcmp(oldId,id)) {
        AgentSend("keyboard-result.txt",L"\\Temp\\MeroKeyboardResult.txt");return;
    }
    /* Persist intent first; a lost reply/restart cannot repeat commands. */
    snprintf(result,sizeof(result),"v=1\ndevice=%s\nid=%s\nresult=interrupted_unknown_do_not_repeat\n",MERO_AGENT_DEVICE,id);
    if(!AgentSign(result,sizeof(result),agentKey)||!AgentWrite(L"\\Temp\\MeroKeyboardResult.txt",result,strlen(result)))return;
    MultiByteToWideChar(CP_ACP,0,target,-1,wide,64);
    foreground=GetForegroundWindow();actual[0]=0;GetClassNameW(foreground,actual,64);
    if(!strcmp(target,"MERO_FOREGROUND")){
        wide[0]=0;
        char name[64];unsigned int j;for(j=0;j<63&&actual[j]&&actual[j]<128;j++)name[j]=(char)actual[j];name[j]=0;
        if(!actual[j]&&MeroKeyboardTarget(name))lstrcpyW(wide,actual);
    }
    if(!wcscmp(actual,wide)&&memchr(bytes,0,len)&&wcscmp(wide,L"MERO_TERMINAL"))
        outcome="unsupported_control";
    else if(!wcscmp(actual,wide)) {
        outcome="queued";
        for(i=0;i<len;) {
            unsigned int used=0,vk=MeroKeyboardEscape(bytes+i,len-i,&used);
            if(GetForegroundWindow()!=foreground){outcome="focus_changed_partial_do_not_repeat";break;}
            if(vk){if(!PostMessageW(foreground,WM_KEYDOWN,vk,0)||!PostMessageW(foreground,WM_KEYUP,vk,0)){outcome="delivery_failed_partial_do_not_repeat";break;}i+=used;}
            else {
                WCHAR ch;unsigned int count=1;unsigned char b=bytes[i];
                if(b>=0xc2&&b<=0xdf)count=2;else if(b>=0xe0&&b<=0xef)count=3;
                if(count>len-i||!MultiByteToWideChar(CP_UTF8,0,(char*)bytes+i,count,&ch,1)){outcome="invalid_text_partial_do_not_repeat";break;}
                if(ch==127)ch=8;
                if((ch==27&&wcscmp(wide,L"MERO_TERMINAL"))||
                   (!wcscmp(wide,L"MeroShellWndClass")&&(ch==13||ch==9))){
                    vk=ch==13?VK_RETURN:ch==9?VK_TAB:VK_ESCAPE;
                    if(!PostMessageW(foreground,WM_KEYDOWN,vk,0)||!PostMessageW(foreground,WM_KEYUP,vk,0)){outcome="delivery_failed_partial_do_not_repeat";break;}
                }else if(!PostMessageW(foreground,WM_CHAR,ch,0)){outcome="delivery_failed_partial_do_not_repeat";break;}
                i+=count;
            }
        }
    }
    snprintf(result,sizeof(result),"v=1\ndevice=%s\nid=%s\nresult=%s\n",MERO_AGENT_DEVICE,id,outcome);
    if(AgentSign(result,sizeof(result),agentKey)&&AgentWrite(L"\\Temp\\MeroKeyboardResult.txt",result,strlen(result)))
        AgentSend("keyboard-result.txt",L"\\Temp\\MeroKeyboardResult.txt");
}
#endif
