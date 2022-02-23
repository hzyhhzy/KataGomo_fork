// options
#define FORBIDDEN_FEATURE  // forbidden points input features
//#define NOVCF                     //if defined, then not use vcf
#define USE_VCF_FEATURE_IF_USE_VCF
//#define CHANGE_FORBIDDEN_POLICY   //set policy target of forbidden points to nonzero
//#define USE_HISTORY               //history features
//#define FORGOMOCUP                //CPU only and single thread




#ifndef COMPILE_MAX_BOARD_LEN
#define COMPILE_MAX_BOARD_LEN 15 // Board::MAX_LEN, Normal gomoku/renju games are on 15x15 board
#endif 