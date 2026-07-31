// resource.h — Kaynak kimlikleri.
//
// Aralıklar bilinçli olarak ayrıktır: yeni bir dize eklerken menü ya da simge
// kimliklerine çarpmamak için.
#pragma once

// --- Simgeler (100-199) ------------------------------------------------------
#define IDI_APP                 101
#define IDI_TRAY_DARK           102
#define IDI_TRAY_LIGHT          103

// --- Tepsi menüsü komutları (200-299) ----------------------------------------
#define IDM_CAPTURE_REGION      201
#define IDM_CAPTURE_WINDOW      202
#define IDM_CAPTURE_FULLSCREEN  203
#define IDM_CAPTURE_DELAYED     204
#define IDM_SELECT_TEXT         205
#define IDM_CAPTURE_OCR         206
#define IDM_PICK_COLOR          207
#define IDM_OPEN_FOLDER         208
#define IDM_SETTINGS            209
#define IDM_ABOUT               210
#define IDM_EXIT                211
#define IDM_HISTORY             212

// --- Dizeler (1000+) ---------------------------------------------------------
// TEK BLOK HÂLİNDE ARTAR: RT_STRING kaynakları 16'lık kümeler hâlinde saklanır,
// bu yüzden kimlikler seyrek değil ARDIŞIK verilir; aradaki her boşluk
// kullanılmayan bir kümeyi ikilide taşımak demektir.

// Tepsi menüsü
#define IDS_MENU_REGION         1000
#define IDS_MENU_WINDOW         1001
#define IDS_MENU_FULLSCREEN     1002
#define IDS_MENU_DELAYED        1003
#define IDS_MENU_SELECT_TEXT    1004
#define IDS_MENU_REGION_TEXT    1005
#define IDS_MENU_PICK_COLOR     1006
#define IDS_MENU_OPEN_FOLDER    1007
#define IDS_MENU_SETTINGS       1008
#define IDS_MENU_ABOUT          1009
#define IDS_MENU_EXIT           1010

// Metin seçme bağlam menüsü
#define IDS_TEXTMENU_COPY       1011
#define IDS_TEXTMENU_COPY_ALL   1012
#define IDS_TEXTMENU_SELECT_ALL 1013
#define IDS_TEXTMENU_CANCEL     1014

// İğne penceresi menüsü
#define IDS_PIN_COPY            1015
#define IDS_PIN_SAVE_AS         1016
#define IDS_PIN_ACTUAL_SIZE     1017
#define IDS_PIN_OPAQUE          1018
#define IDS_PIN_TRANSLUCENT     1019
#define IDS_PIN_CLOSE           1020
#define IDS_PIN_TITLE           1021

// Kaplama ipuçları
#define IDS_HINT_REGION         1022
#define IDS_HINT_COLOR          1023
#define IDS_HINT_TEXT           1024

// İletiler
#define IDS_APP_TITLE           1025
#define IDS_TRAY_TOOLTIP        1026
#define IDS_OCR_UNAVAILABLE     1027
#define IDS_OCR_NO_TEXT_REGION  1028
#define IDS_OCR_NO_TEXT_SCREEN  1029
#define IDS_SAVE_FAILED         1030
#define IDS_START_FAILED        1031
#define IDS_COM_FAILED          1032
#define IDS_ABOUT_BODY          1033
#define IDS_ABOUT_OCR_YES       1034
#define IDS_ABOUT_OCR_NO        1035
#define IDS_ABOUT_TITLE         1036
#define IDS_LANG_AUTO           1037
#define IDS_ABOUT_TAGLINE       1038
#define IDS_EDITOR_TITLE        1039

// Geçmiş penceresi
#define IDS_MENU_HISTORY        1040
#define IDS_HISTORY_TITLE       1041
#define IDS_HISTORY_EMPTY       1042
#define IDS_HISTORY_EDIT        1043
#define IDS_HISTORY_COPY        1044
#define IDS_HISTORY_REVEAL      1045
#define IDS_HISTORY_DELETE      1046
#define IDS_HISTORY_CLEAR       1047
#define IDS_HISTORY_CLEAR_ASK   1048

// Yakalama bildirimi
#define IDS_TOAST_COPIED        1049
#define IDS_TOAST_SAVED         1050
#define IDS_TOAST_COPIED_SAVED  1051
#define IDS_TOAST_PINNED        1052
#define IDS_TOAST_CAPTURED      1053

// Ayarlar penceresi
#define IDS_SETTINGS_TITLE      1054
#define IDS_SET_GROUP_GENERAL   1055
#define IDS_SET_LANGUAGE        1056
#define IDS_SET_THEME           1057
#define IDS_SET_THEME_SYSTEM    1058
#define IDS_SET_THEME_LIGHT     1059
#define IDS_SET_THEME_DARK      1060
#define IDS_SET_GROUP_CAPTURE   1061
#define IDS_SET_DELAY           1062
#define IDS_SET_MAGNIFIER       1063
#define IDS_SET_HIGHLIGHT       1064
#define IDS_SET_PRINTSCREEN     1065
#define IDS_SET_SHUTTER         1066
#define IDS_SET_GROUP_AFTER     1067
#define IDS_SET_AFTER_COPY      1068
#define IDS_SET_AFTER_SAVE      1069
#define IDS_SET_AFTER_PIN       1070
#define IDS_SET_AFTER_EDITOR    1071
#define IDS_SET_NOTIFY          1072
#define IDS_SET_GROUP_SAVE      1073
#define IDS_SET_FOLDER          1074
#define IDS_SET_BROWSE          1075
#define IDS_SET_FORMAT          1076
#define IDS_SET_QUALITY         1077
#define IDS_SET_GROUP_HISTORY   1078
#define IDS_SET_HISTORY_LIMIT   1079
#define IDS_SET_HISTORY_CLEAR   1080
#define IDS_SET_HISTORY_CLEARED 1081
#define IDS_SET_GROUP_HOTKEYS   1082
#define IDS_SET_HK_REGION       1083
#define IDS_SET_HK_WINDOW       1084
#define IDS_SET_HK_FULLSCREEN   1085
#define IDS_SET_HK_DELAYED      1086
#define IDS_SET_OK              1087
#define IDS_SET_CANCEL          1088
#define IDS_SET_RESET           1089
#define IDS_SET_FOLDER_PROMPT   1090

// --- Kısayol etiketleri (1104-1119) ------------------------------------------
// AYRI BLOKTA ve YALNIZCA DİLDEN BAĞIMSIZ tanımlı. "Ctrl+Shift+S" her dilde
// aynı yazılır; 16 tabloya kopyalamak 16 kat bakım demek olurdu.
//
// 1104, RT_STRING blok sınırına (16'nın katı) hizalıdır: blok 70 yalnızca bu
// dizeleri taşır, çevrilmiş dizelerle aynı bloğu paylaşmaz. Paylaşsalardı
// dilden bağımsız blok, çevrilmiş bloğu gölgelerdi.
#define IDS_ACCEL_REGION        1104
#define IDS_ACCEL_WINDOW        1105
#define IDS_ACCEL_FULLSCREEN    1106
#define IDS_ACCEL_DELAYED       1107
#define IDS_ACCEL_COPY          1108
#define IDS_ACCEL_SELECT_ALL    1109
#define IDS_ACCEL_SAVE          1110
#define IDS_ACCEL_ESC           1111
#define IDS_ACCEL_DBLCLICK      1112
