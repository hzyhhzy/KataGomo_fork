#pragma once
#include "NNUEglobal.h"
#include <vector>

namespace NNUEV2 {
    static const int featureHalfLen = 6;
    static const int featureLen = 2 * featureHalfLen + 1;

    const int shapeNum = 4 * 1594323; //4*3^13
    static_assert(featureLen == 13, "shapeNum=4*3^featureLen");
    const int globalFeatureNum           = 39 + 3;//39 for katago, 3 for boardsize
    const int groupSize    = 64;
    const int mlpChannel = 64;


    const int groupBatch = groupSize / 16;
    const int groupBatch32 = groupSize / 8;
    const int featureNum   = groupSize * 2;
    const int featureBatch = featureNum / 16;
    const int trunkconv1GroupSize = 4;

    const int mlpBatch32 = mlpChannel / 8;

    /*
    计算流程：
    1.更新棋形id，将棋形feature更新

    */
    struct ModelWeight
    {
      // 1  mapf = self.mapping(x), shape=H*W*4*2g
      //the mapping is larger than 2GB, so split to 4 parts
      static const int16_t IllegalShapeFeature = 11454;  //any number >6000
      int16_t mapping[shapeNum][featureNum];
      static_assert(shapeNum * featureNum * sizeof(int16_t) < 2100000000, "mapping size limit");

      //illegalmap是落第二个子时所有不能落的子（包括已经有棋子的位置，和因为“优先值”而不能落的位置）
      //illegalBias = illegalmap * illegalVector.view(1, 2 * self.groupc, 1, 1)
      //lb1 = illegalBias[:, : self.groupc]
      //lb2 = illegalBias[:, self.groupc : ]
      int16_t illegalVector[featureNum];


      // 2 
      //  g1=mapf[:,:,:self.groupc,:,:]#第一组通道
      //  g2=mapf[:,:,self.groupc:,:,:]#第二组通道
      //  gfvector=mlp(gf)#gf表示规则
      //  这里的gfvector是python的4倍，因为后续4线平均改成了4线求和
      float gfmlp_w[globalFeatureNum][groupSize];  // shape=(inc，outc)，相同的inc对应权重相邻
      float gfmlp_b[groupSize];

      // 3  
      // g1sum=g1.mean(1) + rv.view(rv.shape[0],rv.shape[1],1,1) + lb1 
      // h1 = self.g1lr(g1sum) #四线求和再加规则向量再leakyrelu
      int16_t g1lr_w[ groupSize];

      // 4  h1 = torch.stack(self.h1conv(h1), dim = 1) #沿着一条线卷积

      int16_t h1conv_w[featureHalfLen + 1][ groupSize]; //卷积核是对称的，所以除2
      int16_t h1conv_b[ groupSize];

      // 5  h2 = self.h1lr2(self.h1lr1(h1, dim = 2) + g2, dim = 2)
      int16_t h1lr1_w[ groupSize];
      int16_t h1lr2_w[ groupSize];

      // 6  h3 = h2.mean(1) + lb2 #最后把四条线整合起来

      // 7  trunk = self.h3lr(h3) 
      int16_t h3lr_w[ groupSize];
      int16_t h3lr_b[ groupSize]; 

      // 8  trunk = self.trunkconv1(trunk) 
      int16_t trunkconv1_w[ trunkconv1GroupSize][ groupSize];
      int16_t trunkconv1_b[ groupSize];

      // 9  trunk = self.trunklr1(trunk) 
      int16_t trunklr1_w[ groupSize];

      // 10 trunk = self.trunkconv2(trunk)
      int16_t trunkconv2_w[3][ groupSize];//对称的3x3卷积

      // 11 trunk = self.trunklr2(trunk) 
      int16_t trunklr2_w[ groupSize];
      int16_t trunklr2_b[ groupSize];


      // 13  v=v.mean((2,3))
      float scale_beforemlpInv;
      float valuelr_w[ groupSize];
      float valuelr_b[ groupSize]; 

      // 14  mlp
      float mlp_w1[groupSize][mlpChannel];  // shape=(inc，outc)，相同的inc对应权重相邻
      float mlp_b1[ mlpChannel];
      float mlp_w2[ mlpChannel][ mlpChannel];
      float mlp_b2[ mlpChannel];
      float mlp_w3[ mlpChannel][ mlpChannel];
      float mlp_b3[ mlpChannel];
      float mlp_w4[mlpChannel][mlpChannel];
      float mlp_b4[mlpChannel];
      float mlpfinal_w[mlpChannel][4];
      float mlpfinal_w_for_safety[4];  // mlp_w3在read的时候一次read
                                   // 8个，会read到后续内存mlp_w3[mix6::valueNum-1][3]+4，
      float mlpfinal_b[4];
      float mlpfinal_b_for_safety[4];  // mlp_b3在read的时候一次read
                                   // 8个，会read到后续内存mlp_b3[3]+4，


      // 15  mlp policy head
      float mlp_p_w[mlpChannel][groupSize];
      float mlp_p_b[groupSize];

      // 16 policy head prelu
      float mlp_plr_w[groupSize];

      // 17 policy=sum(int16(y)*trunk*scale_beforemlpInv)





      bool loadParam(std::string filename);
    };
    struct OnePointChange
    {
      NU_Loc      loc;
      int16_t  dir;
      uint32_t oldshape;
      uint32_t newshape;
    };
    struct ModelBuf
    {
      // 1 convert board to shape
      uint32_t shapeTable[MaxBS * MaxBS][4];  // 4个方向，MaxBS*MaxBS个位置

      // 2  shape到vector  g1无需提取，只缓存g2
      int16_t g2[MaxBS * MaxBS][4][groupSize];

      // 3  g1sum=g1.sum(1), shape=H*W*g
      int16_t g1sum[MaxBS * MaxBS][groupSize];

      // 4  h1=self.g1lr(g1sum), shape=HWc
      //int16_t h1[MaxBS * MaxBS][groupSize];
      int16_t h1m[(MaxBS + 2 * featureHalfLen) * (MaxBS + 2 * featureHalfLen) * (featureHalfLen + 1) * 16];//只是开了一块空间，避免频繁new/delete，每次用的时候清零

      // 后面的部分几乎没法增量计算


      // value头和policy头共享trunk，所以也放在缓存里
      bool    trunkUpToDate;
      int16_t trunk[MaxBS * MaxBS][groupSize];  

      // value头和policy头共享mlp的前三层，所以把第三层的输出也放在缓存里
      float mlp_layer4[mlpChannel];
      float mlp_value[8];//前3个依次是胜负和，第4个是pass的policy，最后4个仅为保留内存

      void update(Color oldcolor, Color newcolor, NU_Loc loc, const ModelWeight &weights);

      void emptyboard(const ModelWeight &weights);  // init
    };

}  // namespace NNUEV2
class Eva_nnuev2
{
public:
  Color mySide;  //这个估值器是哪边的。无论上一手是谁走的，都返回下一手为mySide的估值
  Color board[MaxBS * MaxBS];

  uint64_t         TotalEvalNum;
  NNUEV2::ModelWeight weights;
  NNUEV2::ModelBuf buf;

  bool loadParam(std::string filepath);
  void clear();
  void recalculate();  //根据board完全重新计算棋形表

  //计算拆分为两部分，第一部分是可增量计算的，放在play函数里。第二部分是不易增量计算的，放在evaluate里。
  void      play(Color color, NU_Loc loc);
  NNUE::ValueType evaluateFull(const float *gf, const bool* illegalMap, NNUE::PolicyType *policy);  // policy通过函数参数返回
  void evaluatePolicy(const float *gf, const bool* illegalMap, NNUE::PolicyType *policy);  // policy通过函数参数返回
  NNUE::ValueType evaluateValue(const float *gf, const bool* illegalMap);                //

  void undo(NU_Loc loc);  // play的逆过程

  void debug_print();

private:
  void calculateTrunk(const float *gf, const bool *illegalMap);
};
