#!/usr/bin/env python3
"""Build EncrypticDemo.rbxmx for drag-and-drop import into Roblox Studio."""

from pathlib import Path
import xml.sax.saxutils as sax

ROOT = Path(__file__).resolve().parent.parent
ENCRYPTIC = ROOT / "ServerScriptService" / "Encryptic"
OUT = Path(__file__).resolve().parent / "EncrypticDemo.rbxmx"

MODULES = [
    "init",
    "BanManager",
    "MovementGuard",
    "TeleportGuard",
    "HumanoidGuard",
    "FlyGuard",
    "NoclipGuard",
    "FireRateGuard",
    "RemoteGuard",
    "ExploitGuard",
    "ToolGuard",
    "HitGuard",
]

SCRIPTS = [
    ("ModuleScript", ENCRYPTIC / f"{name}.lua", name)
    for name in MODULES
] + [
    ("Script", Path(__file__).resolve().parent / "ServerScriptService" / "DemoGame.server.lua", "DemoGame"),
    (
        "LocalScript",
        Path(__file__).resolve().parent / "StarterPlayer" / "StarterPlayerScripts" / "DemoClient.client.lua",
        "DemoClient",
    ),
    (
        "LocalScript",
        Path(__file__).resolve().parent / "StarterPlayer" / "StarterPlayerScripts" / "ExploitTestGUI.client.lua",
        "ExploitTestGUI",
    ),
]


def cdata(text: str) -> str:
    return text.replace("]]>", "]]]]><![CDATA[>")


def item(class_name: str, name: str, source: str | None, referent: str, indent: str) -> str:
    lines = [
        f'{indent}<Item class="{class_name}" referent="{referent}">',
        f"{indent}  <Properties>",
        f"{indent}    <string name=\"Name\">{sax.escape(name)}</string>",
    ]
    if source is not None:
        lines.append(f"{indent}    <ProtectedString name=\"Source\"><![CDATA[{cdata(source)}]]></ProtectedString>")
    lines.extend([f"{indent}  </Properties>", f"{indent}</Item>"])
    return "\n".join(lines)


def main() -> None:
    ref = 0
    parts = ['<?xml version="1.0" encoding="utf-8"?>', '<roblox version="4">']

    ref += 1
    model_ref = f"RBX{ref}"
    parts.append(f'  <Item class="Model" referent="{model_ref}">')
    parts.append("    <Properties>")
    parts.append("      <string name=\"Name\">EncrypticDemo</string>")
    parts.append("    </Properties>")

    ref += 1
    folder_ref = f"RBX{ref}"
    parts.append(f'    <Item class="Folder" referent="{folder_ref}">')
    parts.append("      <Properties>")
    parts.append("        <string name=\"Name\">Encryptic</string>")
    parts.append("      </Properties>")

    module_count = len(MODULES)
    for class_name, path, name in SCRIPTS[:module_count]:
        ref += 1
        source = path.read_text(encoding="utf-8")
        parts.append(item(class_name, name, source, f"RBX{ref}", "      "))

    parts.append("    </Item>")

    for class_name, path, name in SCRIPTS[module_count:]:
        ref += 1
        source = path.read_text(encoding="utf-8")
        parts.append(item(class_name, name, source, f"RBX{ref}", "    "))

    parts.append("  </Item>")
    parts.append("</roblox>")

    OUT.write_text("\n".join(parts) + "\n", encoding="utf-8")
    print(f"Wrote {OUT}")


if __name__ == "__main__":
    main()
