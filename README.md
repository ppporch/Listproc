# ListProc 8.2 Source Archive

This repository is a preservation import of the ListProc 8.2 source tree that was
passed along from SourceForge-era materials.

ListProc, also called ListProcessor, is a Unix mailing list and file archive
manager originally maintained by CREN. It includes mail intake, list delivery,
request processing, digest generation, file archive support, and an optional
interactive TCP service.

## Current Status

This is historical software. It is not currently ready for deployment on a
modern system.

The source tree has been placed under git so it can be reviewed, archived, and
modernized incrementally. The first local build check on macOS did not complete:
the legacy makefiles expect a generated or local `makeincludes/Makeinclude.local`
file and a configured `SRCDIR` value.

## Important Security Notice

Do not run this code on a networked machine, and do not install it with elevated
privileges, without a hardening pass.

Initial review found several old-C and old-Unix risks that are typical for this
era of software:

- The installer sets `catmail` setuid, and the documentation describes setuid
  use for `serverd` when binding privileged ports.
- Temporary files are created through name-generation patterns that are not safe
  by modern standards.
- Several paths build shell commands from formatted strings.
- The code uses many unchecked `sprintf`, `strcpy`, and `strcat` calls.
- The optional interactive service listens for TCP connections and predates
  modern expectations for exposed services.

Treat the repository as archival until those areas are addressed.

## Repository Layout

- `bins/` - main programs such as `listproc`, `serverd`, `catmail`, `list`,
  `farch`, `ilp`, and helper binaries.
- `commands/` - request/command handlers used by ListProc.
- `lplib/` - ListProc library code and shared data structures.
- `lputil/` - utility library code for files, logging, locking, strings,
  process execution, and parsing.
- `objects/` - mailbox, subscriber, message, peer, and list object code.
- `send/` - SMTP, mail queue, notification, and delivery helpers.
- `port/` - platform-specific lock/system definitions.
- `makeincludes/` - legacy platform make include files.
- `installation/` - installation scripts, default text tree, man pages, help
  files, and upgrade notes.
- `scripts/` - contributed and support scripts from the historical tree.
- `user-docs/`, `owner-docs/`, `manager-faq/`, `notes/` - bundled
  documentation.

## Build Notes

The historical build system is makefile-based and platform-specific. It expects
environment/configuration not present in this import, notably:

- `SRCDIR` pointing to the source tree.
- `makeincludes/Makeinclude.local`, normally selected or generated from one of
  the platform-specific files in `makeincludes/`.
- A compatible Unix-like build environment and legacy assumptions about system
  headers, tools, and mail infrastructure.

On macOS, a plain `make` currently fails before compilation because those legacy
build variables/files are missing.

## Documentation

Useful starting points:

- `installation/README.INSTALL`
- `installation/RELEASE-NOTES`
- `installation/README.CVS`
- `CHANGES`
- `installation/text/doc/`
- `installation/text/help/`

## License and Legal Notes

The original distribution includes `license`, `license.txt`, `LEGAL`, and
`LEGAL.txt`. Those files are preserved unchanged. `LEGAL` notes that a portion
of the covered code is owned by Sleepycat Software.

Review the included license files before redistribution or reuse.

## Modernization Checklist

Suggested first steps before attempting real operation:

1. Reconstruct a reproducible build without setuid installation.
2. Replace unsafe temporary-file creation with `mkstemp` or equivalent.
3. Remove or isolate shell-command execution paths.
4. Replace unchecked string formatting/copying with bounded alternatives.
5. Disable the interactive TCP listener by default.
6. Add compiler warnings, static analysis, and basic regression tests.
7. Document a non-root, local-only test harness.

