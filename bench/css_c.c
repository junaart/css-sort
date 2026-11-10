/* SPDX-License-Identifier: MIT */
/* CSS со счётчиком сравнений; символы переименованы, чтобы сборка уживалась с css_t.c */
#define CSS_NO_MAIN
#define CSS_COUNT
#define css_sort_ex cssc_sort_ex
#define css_sort cssc_sort
#define css_sort_tab cssc_sort_tab
#define css_table_from_sorted cssc_table_from_sorted
#define css_release cssc_release
#include "../css.c"
