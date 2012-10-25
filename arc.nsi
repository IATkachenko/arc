;ARC Windows Installer
Name "Advanced Resource Connector - ARC"
OutFile "arcsetup.exe"
!include "XML.nsh"
InstallDir $PROGRAMFILES\ARC
; Pages
Page components
Page directory
Page instfiles

; Modifiy these variables if you deployed GTK and MSYS elsewhere
!define GTKPATH "C:\GTK"
!define MSYSPATH "C:\msys"
!define OPENSSLPATH "C:\WINDOWS\system32"

UninstPage uninstConfirm
UninstPage instfiles

; Main components = arched.exe and required libraries
Section "Main Components"
	SetOutPath $INSTDIR
	File /oname=arched.exe "src\hed\daemon\.libs\arched.exe"
	; File /oname=test.exe "src\tests\echo\.libs\test.exe"
	; File /oname=echo_service.xml "src\tests\echo\service.xml.win32"
	; File /oname=accesslist "src\tests\echo\accesslist"
	; File /oname=Policy_Example.xml "src\tests\echo\Policy_Example.xml"
	; File /oname=ca.pem "src\tests\echo\ca.pem"
	; File /oname=key.pem "src\tests\echo\key.pem"
	; File /oname=cert.pem "src\tests\echo\cert.pem"

	; Arc Libs
	File /oname=libarccommon-0.dll "src\hed\libs\common\.libs\libarccommon-0.dll"
	File /oname=libarcloader-0.dll "src\hed\libs\loader\.libs\libarcloader-0.dll"
	File /oname=libarcmessage-0.dll "src\hed\libs\message\.libs\libarcmessage-0.dll"
	File /oname=libarcsecurity-0.dll "src\hed\libs\security\.libs\libarcsecurity-0.dll"
	File /oname=libarccommunication-0.dll "src\hed\libs\communication\.libs\libarccommunication-0.dll"
	File /oname=libarcclient-0.dll "src\hed\libs\client\.libs\libarcclient-0.dll"
	File /oname=libarcws-0.dll "src\hed\libs\ws\.libs\libarcws-0.dll"
	File /oname=libarcdata-0.dll "src\hed\libs\data\.libs\libarcdata-0.dll"
    ; GLIB libs
	File /oname=libgnurx-0.dll "${MSYSPATH}\lib\libgnurx-0.dll"
	File /oname=libglib-2.0-0.dll "${GTKPATH}\bin\libglib-2.0-0.dll"
	File /oname=libgmodule-2.0-0.dll "${GTKPATH}\bin\libgmodule-2.0-0.dll"
	File /oname=libgobject-2.0-0.dll "${GTKPATH}\bin\libgobject-2.0-0.dll"
	File /oname=libgthread-2.0-0.dll "${GTKPATH}\bin\libgthread-2.0-0.dll"
	File /oname=intl.dll "${GTKPATH}\bin\intl.dll"
	File /oname=iconv.dll "${GTKPATH}\bin\iconv.dll"
	File /oname=libglibmm-2.4-1.dll "${GTKPATH}\bin\libglibmm-2.4-1.dll"
	File /oname=libsigc-2.0-0.dll "${GTKPATH}\bin\libsigc-2.0-0.dll"
	File /oname=libxml2.dll "${GTKPATH}\bin\libxml2.dll"
	File /oname=zlib1.dll "${GTKPATH}\bin\zlib1.dll"
    ; OpenSSL
    File /oname=libeay32.dll "${OPENSSLPATH}\libeay32.dll"
    File /oname=libssl32.dll "${OPENSSLPATH}\libssl32.dll"
    File /oname=ssleay32.dll "${OPENSSLPATH}\ssleay32.dll"
	WriteUninstaller "uninstall.exe"
SectionEnd

Section "Plugins"
	CreateDirectory $INSTDIR\plugins\mcc
	File /oname=plugins\mcc\libmcctcp.dll "src\hed\mcc\tcp\.libs\libmcctcp.dll"
	; File /oname=plugins\mcc\libmcctls.dll "src\hed\mcc\tls\.libs\libmcctls.dll"
	File /oname=plugins\mcc\libmcchttp.dll "src\hed\mcc\http\.libs\libmcchttp.dll"
	File /oname=plugins\mcc\libmccsoap.dll "src\hed\mcc\soap\.libs\libmccsoap.dll"
	; CreateDirectory $INSTDIR\plugins\pdc
	; File /oname=plugins\pdc\libarcshc.dll "src\hed\pdc\.libs\libshcpdc.dll"

	CreateDirectory $INSTDIR\plugins\dmc
	File /oname=plugins\dmc\libdmcfile.dll "src\hed\dmc\file\.libs\libdmcfile.dll"
	File /oname=plugins\dmc\libdmchttp.dll "src\hed\dmc\http\.libs\libdmchttp.dll"
SectionEnd

Section "Services"
	CreateDirectory $INSTDIR\services
	; File /oname=services\libecho.dll "src\tests\echo\.libs\libecho.dll"
	; Paul service
    File /oname=services\libpaul.dll "src\services\paul\.libs\libpaul.dll"
    File /oname=paul_gui.exe "src\hed\daemon\win32\.libs\paul_gui.exe"
    File /oname=paul_gui_template.xml "src\hed\daemon\win32\paul_gui.xml"
    File /oname=arc.ico "src\hed\daemon\win32\arc.ico"
    File /oname=style.css "src\services\paul\style.css"
    File /oname=paul_service_template.xml "src\services\paul\paul_service_win32.xml"
    ; Create paul_gui.xml
    ${xml::LoadFile} "paul_gui_template.xml" $0
    ${xml::GotoPath} "/PaulGUI/ArcPluginPath" $0 
    ${xml::SetText} "$INSTDIR\plugins\mcc;$INSTDIR\plugins\dmc" $0
    ${xml::GotoPath} "/PaulGUI/ArcHEDConfig" $0
    ${xml::SetText} "$INSTDIR\paul_service.xml" $0
    ${xml::GotoPath} "/PaulGUI/ArcHEDCmd" $0
    ${xml::SetText} "$INSTDIR\arched.exe" $0
    ${xml::SaveFile} "paul_gui.xml" $0
    ${xml::Unload}
    ; Create paul_service.xml
    ${xml::LoadFile} "paul_service_template.xml" $0
    ${xml::RootElement} $0 $1
    ${xml::XPathNode} "//Service[@id='paul']/paul:JobRoot" $0
    ${xml::SetText} "$INSTDIR\jobroot" $0
    ${xml::SaveFile} "paul_service.xml" $0
    ${xml::Unload}
SectionEnd

Section "Clients"
    File /oname=arccp.exe "src\clients\data\.libs\arccp.exe"
SectionEnd

Section "Uninstall"
	Delete "$INSTDIR\plugins\mcc\*.*"
	RMDir $INSTDIR\plugins\mcc
	Delete "$INSTDIR\plugins\pdc\*.*"
	RMDir $INSTDIR\plugins\pdc
	Delete "$INSTDIR\plugins\dmc\*.*"
	RMDir $INSTDIR\plugins\dmc
	RMDir $INSTDIR\plugins
	Delete "$INSTDIR\services\*.*"
	RMDir $INSTDIR\services
	Delete "$INSTDIR\*.*"
	RMDir $INSTDIR
SectionEnd

