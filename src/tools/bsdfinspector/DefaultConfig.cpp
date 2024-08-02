#include "DefaultConfig.h"

#include "UI.h"

namespace IG {
static const char* DefaultIni = R"""(
[Window][WindowOverViewport_11111111]
Pos=0,32
Size=2167,1187
Collapsed=0

[Window][Properties]
Pos=1817,32
Size=350,1187
Collapsed=0
DockId=0x00000002,0

[Window][Debug##Default]
Pos=60,60
Size=400,400
Collapsed=0

[Window][View]
Pos=0,32
Size=1814,1187
Collapsed=0
DockId=0x00000001,0

[Docking][Data]
DockSpace   ID=0x7C6B3D9B Window=0xA87D555D Pos=0,32 Size=2167,1187 Split=X
  DockNode  ID=0x00000001 Parent=0x7C6B3D9B SizeRef=1814,1187 CentralNode=1 Selected=0x530C566C
  DockNode  ID=0x00000002 Parent=0x7C6B3D9B SizeRef=350,1187 Selected=0x199AB496
)""";

void LoadDefaultConfig()
{
    ImGui::LoadIniSettingsFromMemory(DefaultIni);
}
} // namespace IG