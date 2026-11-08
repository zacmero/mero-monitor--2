# Embedded Cascadia Code Nerd Font bitmap

Source: the user's installed CaskaydiaCove Nerd Font Mono Regular.
The generated header records the exact source file SHA256.
Generated with scripts/generate_terminal_font.py using Pillow and fontconfig.
Only bounded 8x14 bitmap cells are embedded; no system font is replaced.

Cascadia Code copyright and SIL Open Font License are reproduced in
CASCADIA_FONT_LICENSE.txt. The earlier DejaVu atlas license remains in
FONT_LICENSE.txt for provenance.

Nerd Fonts patches combine icon sets with their respective attribution and
licenses. The upstream patcher license is reproduced in NERD_FONTS_LICENSE.txt,
and the icon attribution table is preserved in NERD_FONT_UPSTREAM_README.md.
See the upstream per-font attribution table and licensing files:
- https://github.com/ryanoasis/nerd-fonts/tree/master/patched-fonts/CascadiaCode
- https://github.com/ryanoasis/nerd-fonts/blob/master/LICENSE

The atlas retains Nerd Font BMP private-use glyphs and maps supplementary
U+F0000..U+F2FFF into reserved wire cells U+A000..U+CFFF. These wire codes
are for display only; keyboard input continues to use original UTF-8.
