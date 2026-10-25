"""Enforce the Phase 0 platform boundary (docs/LINUX_PORT.md).

src/game/ must stay OS-agnostic: no Windows headers, no live Win32 API calls.
Comments are exempt -- this project deliberately documents the original
binary's Win32 calls in comments, and those must keep saying what the original
did.

Usage:  python tests/check_platform_boundary.py [--dir src/game]
Exit code 0 = clean, 1 = violations.
"""
import os
import re
import sys

FORBIDDEN_INCLUDE = re.compile(
    r'#\s*include\s*<(windows|mmsystem|d3d11|dxgi|d3dcompiler|xinput|xaudio2|'
    r'dbghelp|imm32|shellapi|shlobj|commdlg)\.h>'
)

# Live Win32 API calls. OutputDebugStringA is deliberately absent: Globals.h
# #defines it to dbg_safe_str (see DebugPrint.h for why).
FORBIDDEN_API = re.compile(
    r'\b(GetAsyncKeyState|GetKeyboardState|MapVirtualKeyA?|ToAscii|'
    r'timeGetTime|timeBeginPeriod|timeEndPeriod|GetTickCount|QueryPerformanceCounter|'
    r'CreateDirectoryA|GetFileAttributesA|FindFirstFileA|FindNextFileA|'
    r'CreateFileA|ReadFile|WriteFile|GetFileSize|GetModuleFileNameA|'
    r'MessageBoxA|IsDebuggerPresent|GetEnvironmentVariableA|'
    r'VirtualAlloc|VirtualProtect|VirtualQuery|ExitProcess|'
    r'waveOut[A-Za-z]*|mciSendStringA|joyGet[A-Za-z]*|XInputGet[A-Za-z]*|'
    r'GetPrivateProfile[A-Za-z]*|WritePrivateProfile[A-Za-z]*|'
    r'RegOpenKey[A-Za-z]*|RegQueryValue[A-Za-z]*|RegSetValue[A-Za-z]*|'
    r'RegCreateKey[A-Za-z]*|CreateWindowExA|PeekMessageA|DispatchMessageA|'
    r'ImmAssociateContext|SystemParametersInfoA|ShowCursor)\b'
)

BLOCK_COMMENT = re.compile(r'/\*.*?\*/', re.S)


def strip_comments(text):
    """Remove // and /* */ comments, preserving line numbering."""
    text = BLOCK_COMMENT.sub(lambda m: '\n' * m.group(0).count('\n'), text)
    out = []
    for line in text.split('\n'):
        cut = line.find('//')
        out.append(line if cut < 0 else line[:cut])
    return out


def scan(root):
    violations = []
    for dirpath, _dirnames, filenames in os.walk(root):
        for name in filenames:
            if not name.endswith(('.cpp', '.h')):
                continue
            path = os.path.join(dirpath, name)
            with open(path, 'r', encoding='utf-8', errors='replace') as f:
                text = f.read()
            lines = strip_comments(text)
            for i, line in enumerate(lines, 1):
                for rx, what in ((FORBIDDEN_INCLUDE, 'Windows header'),
                                 (FORBIDDEN_API, 'Win32 call')):
                    m = rx.search(line)
                    if m:
                        violations.append('%s:%d: %s: %s'
                                          % (path.replace('\\', '/'), i, what,
                                             m.group(0)))
    return violations


def main():
    root = 'src/game'
    if '--dir' in sys.argv:
        root = sys.argv[sys.argv.index('--dir') + 1]
    violations = scan(root)
    if violations:
        print('Platform boundary violated (%s):' % root)
        for v in violations:
            print('  ' + v)
        print('\nOS services belong behind src/platform/platform.h -- see '
              'docs/LINUX_PORT.md Phase 0.')
        return 1
    print('Platform boundary OK: %s has no Windows headers or Win32 calls.' % root)
    return 0


if __name__ == '__main__':
    sys.exit(main())
