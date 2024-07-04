#include "DefaultConfig.h"

#include "UI.h"

namespace IG {
static const char* DefaultIni = R"""(
[Window][WindowOverViewport_11111111]
Pos=0,27
Size=1280,693
Collapsed=0

[Window][Render]
Pos=0,27
Size=927,693
Collapsed=0
DockId=0x00000001,0

[Window][Debug##Default]
Pos=60,60
Size=400,400
Collapsed=0

[Window][Overview]
Pos=930,27
Size=350,393
Collapsed=0
DockId=0x00000003,0

[Window][Parameters]
Pos=930,423
Size=350,297
Collapsed=0
DockId=0x00000004,0

[Window][Loading scene]
Pos=490,338
Size=300,43
Collapsed=0

[Docking][Data]
DockSpace     ID=0x7C6B3D9B Window=0xA87D555D Pos=0,27 Size=1280,693 Split=X Selected=0x81AED595
  DockNode    ID=0x00000001 Parent=0x7C6B3D9B SizeRef=927,693 CentralNode=1 Selected=0x81AED595
  DockNode    ID=0x00000002 Parent=0x7C6B3D9B SizeRef=350,693 Split=Y Selected=0xEF9DBE80
    DockNode  ID=0x00000003 Parent=0x00000002 SizeRef=350,393 Selected=0x0976B96A
    DockNode  ID=0x00000004 Parent=0x00000002 SizeRef=350,297 Selected=0xEF9DBE80
)""";

void LoadDefaultConfig()
{
    ImGui::LoadIniSettingsFromMemory(DefaultIni);
}
} // namespace IG