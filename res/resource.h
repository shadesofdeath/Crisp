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
#define IDM_CAPTURE_ACTIVE      213
#define IDM_CAPTURE_ALL         214
#define IDM_CAPTURE_LAST        215
#define IDM_OPEN_CLIPBOARD      216

// Son yüklenen bağlantılar. Alt menüdeki her satır bir komut kimliği alır ve
// tıklandığında o bağlantıyı panoya kopyalar; blok hâlinde ayrılmalarının
// sebebi, kaç kayıt gösterileceğinin çalışma zamanında belli olması.
#define IDM_LINK_CLEAR          217
#define IDM_LINK_FIRST          260
#define IDM_LINK_LAST           (IDM_LINK_FIRST + 9)

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
#define IDS_HOTKEY_FAILED       1091
#define IDS_MSG_YES             1092
#define IDS_MSG_NO              1093

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

// --- Düzenleyici ipuçları (1120+) --------------------------------------------
// AYRI BLOK: 1120, RT_STRING blok sınırına hizalıdır (16'nın katı) ve bu
// yüzden dilden bağımsız kısayol bloğuyla (1104-1119) aynı kümeyi paylaşmaz.
// Paylaşsalardı, çevrilmemiş blok çevrilmiş olanı gölgelerdi.
#define IDS_TIP_ARROW           1120
#define IDS_TIP_RECTANGLE       1121
#define IDS_TIP_ELLIPSE         1122
#define IDS_TIP_PEN             1123
#define IDS_TIP_HIGHLIGHTER     1124
#define IDS_TIP_TEXT            1125
#define IDS_TIP_STEP            1126
#define IDS_TIP_BLUR            1127
#define IDS_TIP_MOSAIC          1128
#define IDS_TIP_CROP            1129
#define IDS_TIP_ZOOM_OUT        1130
#define IDS_TIP_ZOOM_IN         1131
#define IDS_TIP_ZOOM_FIT        1132
#define IDS_TIP_OCR             1133
#define IDS_TIP_ROTATE_LEFT     1134
#define IDS_TIP_ROTATE_RIGHT    1135
#define IDS_TIP_SCALE           1136
#define IDS_TIP_UNDO            1137
#define IDS_TIP_REDO            1138
#define IDS_TIP_CLEAR           1139
#define IDS_TIP_COPY            1140
#define IDS_TIP_SAVE            1141
#define IDS_TIP_CLOSE           1142
#define IDS_TIP_COLOR           1143
#define IDS_TIP_THICKNESS       1144
#define IDS_TIP_SAVE_AS         1145
#define IDS_TIP_ZOOM_SLIDER     1146

// Araç çubuğu grup etiketleri — düğme sırasının altında yazarlar.
#define IDS_GROUP_TOOLS         1147
#define IDS_GROUP_STYLE         1148
#define IDS_GROUP_IMAGE         1149
#define IDS_GROUP_EDIT          1150
#define IDS_GROUP_FILE          1151

// Düzenleyicinin kendi geri bildirimi
#define IDS_TEXT_HINT           1152
#define IDS_EDITOR_COPIED       1153
#define IDS_EDITOR_SAVED        1154
#define IDS_COPY_FAILED         1155

// Yeni yakalama eylemleri ve ayarları
#define IDS_MENU_CLIPBOARD      1156
#define IDS_OPEN_FAILED         1157
#define IDS_TOAST_COUNTDOWN     1158
#define IDS_SET_CURSOR          1159
#define IDS_SET_DIM             1160
#define IDS_SET_NAME_FORMAT     1161
#define IDS_SET_SUBFOLDER       1162
#define IDS_SET_GROUP_ANNOTATE  1163
#define IDS_SET_BLUR_STRENGTH   1164
#define IDS_SET_MOSAIC_STRENGTH 1165
#define IDS_SET_AFTER_COPYPATH  1166
#define IDS_SET_AFTER_COPYFILE  1167
#define IDS_SET_AFTER_REVEAL    1168
#define IDS_SET_AFTER_OCR       1169

// Kısayol eylemlerinin adları
#define IDS_ACT_NONE            1170
#define IDS_ACT_REGION          1171
#define IDS_ACT_WINDOW          1172
#define IDS_ACT_ACTIVE_WINDOW   1173
#define IDS_ACT_MONITOR         1174
#define IDS_ACT_ALL_MONITORS    1175
#define IDS_ACT_LAST_REGION     1176
#define IDS_ACT_DELAYED         1177
#define IDS_ACT_SELECT_TEXT     1178
#define IDS_ACT_REGION_TEXT     1179
#define IDS_ACT_PICK_COLOR      1180
#define IDS_ACT_HISTORY         1181

// Yeni düzenleyici araçları ve efekt menüsü
#define IDS_TIP_SELECT          1182
#define IDS_TIP_LINE            1183
#define IDS_TIP_EFFECTS         1184
#define IDS_TIP_FILL            1185
#define IDS_IMG_FLIP_H          1186
#define IDS_IMG_FLIP_V          1187
#define IDS_IMG_AUTOCROP        1188
#define IDS_IMG_PADDING         1189
#define IDS_IMG_GRAYSCALE       1190
#define IDS_IMG_INVERT          1191
#define IDS_IMG_SEPIA           1192
#define IDS_IMG_SHARPEN         1193
#define IDS_IMG_BRIGHTNESS      1194
#define IDS_IMG_CONTRAST        1195
#define IDS_IMG_SATURATION      1196

// Explorer sağ tık menüsü
#define IDS_SET_SHELL_MENU      1197
#define IDS_SHELL_VERB          1198
#define IDS_SHELL_FAILED        1199

// Kısayol kutularının altındaki kural satırı
#define IDS_SET_HOTKEY_HINT     1200

// Görsel yükleme
#define IDS_SET_GROUP_UPLOAD    1201
#define IDS_SET_UPLOAD_SERVICE  1202
#define IDS_SET_UPLOAD_KEY      1203
#define IDS_SET_UPLOAD_HINT     1204
#define IDS_SET_UPLOAD_NONE     1205
#define IDS_UPLOAD_WORKING      1206
#define IDS_UPLOAD_COPIED       1207
#define IDS_UPLOAD_FAILED       1208
#define IDS_ED_UPLOAD           1209
#define IDS_SET_AFTER_UPLOAD    1210

// Yükleme hataları. Çekirdek bir UploadError kodu döndürür (crisp_core Loc'a
// bağımlı olamaz); çeviri burada, uygulama katmanında yapılır.
#define IDS_UPERR_NO_SERVICE    1211
#define IDS_UPERR_MISSING_KEY   1212
#define IDS_UPERR_NO_IMAGE      1213
#define IDS_UPERR_NETWORK       1214
#define IDS_UPERR_TLS           1215
#define IDS_UPERR_REJECTED      1216
#define IDS_UPERR_TOO_LARGE     1217
#define IDS_UPERR_TOO_MANY      1218
#define IDS_UPERR_UNAVAILABLE   1219
#define IDS_UPERR_UNEXPECTED    1220
#define IDS_UPERR_UNREADABLE    1221

// Son bağlantılar alt menüsü
#define IDS_MENU_LINKS          1222
#define IDS_MENU_LINKS_EMPTY    1223
#define IDS_MENU_LINKS_CLEAR    1224
#define IDS_LINK_COPIED         1225

// Yerleşmiş seçimin ipucu: boyutlandırma, taşıma ve onaylama.
#define IDS_HINT_REGION_SETTLED 1226
