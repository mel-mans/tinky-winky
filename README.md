# Tinky Winkey — Windows? What's that?

A 42 cybersecurity project. Two programs: a Windows service and a keylogger that runs under it.

---

## What it does

**svc.exe** — a Windows service named `tinky` that manages the keylogger lifecycle. It registers itself with the Service Control Manager, impersonates a SYSTEM token by duplicating the token from `winlogon.exe`, then launches the keylogger in the background with elevated privileges.

**winkey.exe** — a keylogger that installs a system-wide low-level keyboard hook, detects foreground window changes, and logs every keystroke with timestamps and process information to `winkey.log`.

---

## Requirements

- Windows 10 or higher (tested on Windows 10)
- Must be run inside a Virtual Machine
- Administrator privileges are required for all operations
- Windows Defender may need to be disabled
- Both binaries must be in the same directory

---

## Building

Open a **Developer Command Prompt for VS** (not a regular cmd) and run:

```
nmake
```

This produces `svc.exe` and `winkey.exe` in the current directory.

To clean build artifacts:

```
nmake clean
```

To rebuild from scratch:

```
nmake re
```

---

## Running

All commands must be run as Administrator.

### Install the service

```
svc.exe install
```

Registers the `tinky` service with the Windows Service Control Manager. The service will appear in the SCM but will not be running yet.

Verify:

```
sc.exe queryex type=service state=all | Select-String "SERVICE_NAME: tinky" -Context 0,3
```

Expected output:

```
SERVICE_NAME: tinky
    DISPLAY_NAME: tinky
        TYPE   : 10  WIN32_OWN_PROCESS
        STATE  : 1   STOPPED
```

---

### Start the service

```
svc.exe start
```

The service will:
1. Impersonate a SYSTEM token from `winlogon.exe`
2. Launch `winkey.exe` silently in the background

Verify the service is running:

```
sc.exe queryex type=service state=all | Select-String "SERVICE_NAME: tinky" -Context 0,3
```

Expected:

```
SERVICE_NAME: tinky
    DISPLAY_NAME: tinky
        TYPE   : 10  WIN32_OWN_PROCESS
        STATE  : 4   RUNNING
```

Verify the keylogger is running:

```
tasklist | Select-String "winkey"
```

Expected:

```
winkey.exe   2052 Console   1   4028 Ko
```

---

### Stop the service

```
svc.exe stop
```

Sends a stop signal to the service. The keylogger process is terminated automatically.

---

### Delete the service

```
svc.exe delete
```

Removes the `tinky` service from the SCM entirely. If the keylogger is still running it will be killed first.

Verify cleanup:

```
tasklist | Select-String "winkey"
```

Should return nothing.

---

## Log file

The keylogger writes to `winkey.log` in its working directory.

Format:

```
[DD.MM.YYYY HH:MM:SS] - 'Window Title'
<keystrokes>
```

Example:

```
[01.11.2021 06:58:46] - 'New tab - Google Chrome'
ShiftHey i'm currently on tab 1 of my ShiftGoogle ShiftChrome.

[01.11.2021 06:58:56] - 'Intra Profile Home - Google Chrome'
tinky-winkey\n

[01.11.2021 07:01:23] - 'coconut@DESKTOP-C6PDFQLM: ~'
ShiftWelcome_to kali-linux
```

Special keys are written as human-readable labels: `Shift`, `Ctrl`, `Alt`, `Back`, `\n`, etc. Regular characters are written as-is, locale-aware.

---

## Files

```
tinky-winkey/
├── Makefile
├── svc.c
├── winkey.c
└── README.md
```

`winkey.log` is generated at runtime and should not be committed to the repository.

---

## Mandatory WinAPI functions used

- `OpenSCManager`
- `CreateService`
- `OpenService`
- `StartService`
- `ControlService`
- `CloseServiceHandle`
- `DuplicateTokenEx`

---

## Notes

- Only one instance of `winkey.exe` can run at a time.
- The keylogger is launched and killed exclusively by the service — do not run it manually while the service is active.
- The `winkey.exe` binary must be in the same directory as `svc.exe`.
- If the service fails to start, check that Windows Defender is not blocking the binaries.
