// options
//#define NOVCF                     //if defined, then not use vcf
//#define CHANGE_FORBIDDEN_POLICY   //set policy target of forbidden points to nonzero
//#define FORGOMOCUP                //CPU only and single thread

//有多大比例的训练数据是带着vcf的
#define TRAINING_DATA_VCF_PROB 0.7

//训练数据带着禁手特征的概率
#define TRAINING_DATA_FORBIDDEN_FEATURE_PROB 0.1




#ifndef COMPILE_MAX_BOARD_LEN
#define COMPILE_MAX_BOARD_LEN 15 // Board::MAX_LEN, Normal gomoku/renju games are on 15x15 board
#endif 