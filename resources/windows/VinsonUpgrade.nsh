!macro VinsonCheckUpgrade
  IfFileExists '$INSTDIR\vinson-editor.exe' vinson_upgrade_detected vinson_upgrade_continue

vinson_upgrade_detected:
  DetailPrint '检测到现有安装，将在当前目录覆盖更新：$INSTDIR'
  MessageBox MB_OK|MB_ICONINFORMATION \
    '检测到已安装的 Vinson Editor，将在当前目录覆盖更新：$\r$\n$INSTDIR' \
    /SD IDOK

  nsExec::ExecToStack '$SYSDIR\cmd.exe /D /C $SYSDIR\tasklist.exe /NH /FI "IMAGENAME eq vinson-editor.exe" | $SYSDIR\find.exe /I "vinson-editor.exe" >NUL'
  Pop $1
  Pop $2
  StrCmp $1 '0' vinson_upgrade_running vinson_upgrade_continue

vinson_upgrade_running:
  MessageBox MB_YESNO|MB_ICONEXCLAMATION|MB_DEFBUTTON1 \
    'Vinson Editor 正在运行。是否立即停止程序并继续安装？$\r$\n$\r$\n未保存的内容可能会丢失。' \
    /SD IDNO IDYES vinson_upgrade_stop IDNO vinson_upgrade_cancel

vinson_upgrade_stop:
  DetailPrint '正在停止 Vinson Editor...'
  nsExec::ExecToStack '"$SYSDIR\taskkill.exe" /F /T /IM vinson-editor.exe'
  Pop $1
  Pop $2
  StrCmp $1 '0' vinson_upgrade_stopped vinson_upgrade_stop_failed

vinson_upgrade_stopped:
  Sleep 300
  nsExec::ExecToStack '$SYSDIR\cmd.exe /D /C $SYSDIR\tasklist.exe /NH /FI "IMAGENAME eq vinson-editor.exe" | $SYSDIR\find.exe /I "vinson-editor.exe" >NUL'
  Pop $1
  Pop $2
  StrCmp $1 '0' vinson_upgrade_stop_failed 0
  DetailPrint 'Vinson Editor 已停止，继续覆盖安装。'
  Goto vinson_upgrade_continue

vinson_upgrade_stop_failed:
  MessageBox MB_OK|MB_ICONSTOP \
    '无法停止 Vinson Editor。请手动退出程序后重新运行安装包。' /SD IDOK
  Quit

vinson_upgrade_cancel:
  DetailPrint '用户取消了覆盖安装。'
  Quit

vinson_upgrade_continue:
!macroend
