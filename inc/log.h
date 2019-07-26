#ifndef JOS_INC_LOG_H
#define JOS_INC_LOG_H

#include <inc/stdio.h>

#define STR(x) #x
#define STR_CAT(x) STR(x)
#define log(args...) cprintf(__FILE__":"STR_CAT(__LINE__)": "args)
#define log_expr(expr, fmt) log("("#expr"): "fmt"\n",(expr))
#define log_hex(expr) log_expr(expr, "%x") 
#define log_int(expr) log_expr(expr, "%d")

#endif