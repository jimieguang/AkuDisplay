$dir = 'lvgl_project'
$files = @('main.c','sysinfo.c','sysinfo.h','ui_theme.c','ui_theme.h','ui_pages.c','ui_pages.h',
           'ui_apps.c','ui_apps.h','ui_menu.c','ui_menu.h','ui_state.c','ui_state.h',
           'icons.c','icons.h','Makefile','lv_conf.h',
           'drivers\fbdev.c','drivers\fbdev.h','drivers\evdev.c','drivers\evdev.h')
foreach ($f in $files) {
    $p = Join-Path $dir $f
    if (Test-Path $p) {
        $h = (Get-FileHash -Algorithm MD5 $p).Hash.ToLower()
        '{0}  {1}' -f $h, ($f -replace '\\','/')
    } else {
        'MISSING             {0}' -f $f
    }
}
