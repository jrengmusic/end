/*******************************************************************************
                        Codegen Annotated Source of Truth
————————————————————————————————————————————————————————————————————————————————

            ░░████████████░░████████████░░████████████░░████████████
            ░░████  ░░████░░████  ░░████░░████  ░░████    ░░████
            ░░████        ░░████  ░░████░░████            ░░████
            ░░████        ░░████████████░░████████████    ░░████
            ░░████        ░░████  ░░████        ░░████    ░░████
            ░░████  ░░████░░████  ░░████░░████  ░░████    ░░████
            ░░████████████░░████  ░░████░░████████████    ░░████

————————————————————————————————————————————————————————————————————————————————
                         FOR YOUR EYES ONLY, DO NOT EDIT
********************************************************************************/

!include "MUI2.nsh"

!define PRODUCT "END"
!define VERSION "0.0.1"
!define PUBLISHER "Jubilant Research of Eclectic Novelty Generation"
!define COMPANY "JRENG"
!define THUMBPRINT "8274AC1EA9DEA10AEFF7FD683D5E2BC4B5FAAAF1"
!define UNINSTALLKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT}"

Name "${PRODUCT}"
OutFile "${OUTFILE}"
Unicode True
BrandingText "${COMPANY}"
RequestExecutionLevel admin
InstallDir "$PROGRAMFILES64\JRENG\END"

!system '"${SIGNTOOL}" sign /sha1 ${THUMBPRINT} /fd SHA256 "${ARTEFACT}"' = 0
!finalize '"${SIGNTOOL}" sign /sha1 ${THUMBPRINT} /fd SHA256 "%1"' = 0
!uninstfinalize '"${SIGNTOOL}" sign /sha1 ${THUMBPRINT} /fd SHA256 "%1"' = 0

!define MUI_ICON "${ICON}"
!define MUI_UNICON "${ICON}"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "${RESOURCES}\header.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${RESOURCES}\welcome.bmp"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${LICENSE}"
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "Install"

  SetOutPath "$INSTDIR"
  File "${ARTEFACT}"

  SetShellVarContext current
  SetOutPath "$DOCUMENTS\${COMPANY}\Uninstaller"
  WriteUninstaller "$DOCUMENTS\${COMPANY}\Uninstaller\${PRODUCT} Uninstaller.exe"

  SetRegView 64
  WriteRegStr HKLM "${UNINSTALLKEY}" "DisplayName" "${PRODUCT}"
  WriteRegStr HKLM "${UNINSTALLKEY}" "UninstallString" '"$DOCUMENTS\${COMPANY}\Uninstaller\${PRODUCT} Uninstaller.exe"'
  WriteRegStr HKLM "${UNINSTALLKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINSTALLKEY}" "Publisher" "${PUBLISHER}"
  WriteRegDWORD HKLM "${UNINSTALLKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTALLKEY}" "NoRepair" 1

SectionEnd

Section "Uninstall"

  Delete "$PROGRAMFILES64\JRENG\END\END.exe"
  RMDir "$PROGRAMFILES64\JRENG\END"

  SetRegView 64
  DeleteRegKey HKLM "${UNINSTALLKEY}"

  SetShellVarContext current
  Delete "$DOCUMENTS\${COMPANY}\Uninstaller\${PRODUCT} Uninstaller.exe"
  RMDir "$DOCUMENTS\${COMPANY}\Uninstaller"

SectionEnd
