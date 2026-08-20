# ayfxedit-wx source workspace

This folder contains the new cross-platform implementation (wxWidgets 3.3 + SDL3).

Planned layout:
- app/: wxWidgets UI application
- core/: reusable AYFX domain logic and file formats
- audio/: SDL3 playback backend

Dependencies and local third-party sources:
- ../third_party/dr_libs: utility single-header libraries available to the new code

Current scaffold status:
- wxWidgets app boots on VS2022
- File menu is wired for bank/effect I/O through `BankModel`
	- New bank
	- Load/Save bank (.afb)
	- Save bank without names (.afb)
	- Load/Save current effect (.afx)
- Core AFX/AFB encode/decode has been ported from the legacy editor
