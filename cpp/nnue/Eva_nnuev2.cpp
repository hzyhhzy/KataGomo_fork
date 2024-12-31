#include "Eva_nnuev2.h"

#include "../external/simde/simde_avx2.h"
#include "../external/simde/simde_fma.h"

#include <filesystem>
using namespace NNUE;
using namespace NNUEV2;

void ModelBuf::update(Color oldcolor, Color newcolor, NU_Loc loc, const ModelWeight* weights)
{
  if (loc < 0 || loc >= MaxBS * MaxBS)return;
  trunkUpToDate = false;

  // update shapeTable
  std::vector<OnePointChange> changeTable(4 * featureLen);
  int                         changenum = 0;

  {
    int x0 = loc % MaxBS;
    int y0 = loc / MaxBS;

    int dxs[4] = { 1, 0, 1, 1 };
    int dys[4] = { 0, 1, 1, -1 };

    for (int dir = 0; dir < 4; dir++) {
      for (int dist = -featureHalfLen; dist <= featureHalfLen; dist++) {
        int x = x0 - dist * dxs[dir];
        int y = y0 - dist * dys[dir];
        if (x < 0 || x >= MaxBS || y < 0 || y >= MaxBS)
          continue;
        OnePointChange c;
        c.dir = dir, c.loc = MakeLoc(x, y);
        c.oldshape = shapeTable[c.loc][dir];
        c.newshape = c.oldshape + (newcolor - oldcolor) * pow3[dist + featureHalfLen];
        shapeTable[c.loc][dir] = c.newshape;
        changeTable[changenum] = c;
        changenum++;
      }
    }
  }

  for (int p = 0; p < changenum; p++) {
    OnePointChange c = changeTable[p];

    int y0 = c.loc / MaxBS, x0 = c.loc % MaxBS;

    for (int i = 0; i < groupBatch; i++) {

      // g2 update
      auto neww = simde_mm256_loadu_si256(weights->mapping[c.newshape] + i * 16 + groupSize);
      simde_mm256_storeu_si256(g2[c.loc][c.dir] + i * 16, neww);

      // g1 update
      auto  oldw = simde_mm256_loadu_si256(weights->mapping[c.oldshape] + i * 16);
      neww = simde_mm256_loadu_si256(weights->mapping[c.newshape] + i * 16);
      void* wp = g1sum[c.loc] + i * 16;
      auto  sumw = simde_mm256_loadu_si256(wp);
      sumw = simde_mm256_sub_epi16(sumw, oldw);
      sumw = simde_mm256_add_epi16(sumw, neww);
      simde_mm256_storeu_si256(wp, sumw);



    }
  }
}

//变量命名与python训练代码相同，阅读时建议与python代码对照
void Eva_nnuev2::calculateTrunk(const float* gf, const bool* illegalMap)
{
  int16_t rv[groupSize];//规则向量

  //rv=self.gfVector(gf)
  {

    // linear
    for (int i = 0; i < groupBatch32; i++) {
      auto sum = simde_mm256_loadu_ps(weights->gfmlp_b + i * 8);
      for (int j = 0; j < globalFeatureNum; j++) {
        auto x = simde_mm256_set1_ps(gf[j]);
        auto w = simde_mm256_loadu_ps(weights->gfmlp_w[j] + i * 8);
        sum    = simde_mm256_fmadd_ps(w, x, sum);
      }
      auto      sum_int = simde_mm256_cvtps_epi32(sum);
      sum_int      = simde_mm256_packs_epi32(sum_int,sum_int);
      sum_int           = simde_mm256_permute4x64_epi64(sum_int, 0b00001000);
      simde_mm_storeu_si128(rv + i * 8, simde_mm256_extractf128_si256(sum_int,0));
    }

  }

  //for (int i = 0; i < groupSize; i++)
  //    std::cout << rv[i] << " ";
  //std::cout << "\n";

  
  float vsum[groupSize];//sum of trunk, mlp input

  for (int batch = 0; batch < groupBatch; batch++) {  //一直到trunk计算完毕，不同batch之间都没有交互,所以放在最外层
    int addrBias = batch * 16;


    auto rv_batch = simde_mm256_loadu_si256(rv + addrBias);//gfVector
    auto iv1_batch = simde_mm256_loadu_si256(weights->illegalVector + addrBias);//illegalVector

    //这个数组太大，就不直接int16_t[(MaxBS + 10) * (MaxBS + 10)][6][16]了
    //int16_t *h1m = new int16_t[(MaxBS + 10) * (MaxBS + 10)*6*16];  //完整的卷积是先乘再相加，此处是相乘但还没相加。h1m沿一条线相加得到h1c。加了5层padding方便后续处理
    //h1m的定义移到Eva_nnuev2类内了
    memset(buf.h1m, 0, sizeof(int16_t) * (MaxBS + 2 * featureHalfLen) * (MaxBS + 2 * featureHalfLen) * (featureHalfLen + 1) * 16);

    //-------------------------------------------------------------------------------------------------------------------------------------------------------------------
    // g1 prelu和h1conv的乘法部分
    auto g1lr_w   = simde_mm256_loadu_si256(weights->g1lr_w + addrBias);
    auto h1conv_w0 = simde_mm256_loadu_si256(weights->h1conv_w[0] + addrBias);
    auto h1conv_w1 = simde_mm256_loadu_si256(weights->h1conv_w[1] + addrBias);
    auto h1conv_w2 = simde_mm256_loadu_si256(weights->h1conv_w[2] + addrBias);
    auto h1conv_w3 = simde_mm256_loadu_si256(weights->h1conv_w[3] + addrBias);
    auto h1conv_w4 = simde_mm256_loadu_si256(weights->h1conv_w[4] + addrBias);
    auto h1conv_w5 = simde_mm256_loadu_si256(weights->h1conv_w[5] + addrBias);
    auto h1conv_w6 = simde_mm256_loadu_si256(weights->h1conv_w[6] + addrBias);
    static_assert(featureHalfLen == 6, "h1conv group = featureHalfLen + 1");
    for (NU_Loc locY = 0; locY < MaxBS; locY++) {
      for (NU_Loc locX = 0; locX < MaxBS; locX++) {
        NU_Loc loc1 = locY * MaxBS + locX;             //原始loc
        NU_Loc loc2 = (locY + featureHalfLen)  * (MaxBS + 2 * featureHalfLen) + locX + featureHalfLen;  // padding后的loc
        int16_t* h1mbias = buf.h1m + loc2 * (featureHalfLen + 1) * 16;

        auto g1sum = simde_mm256_loadu_si256(buf.g1sum[loc1] + addrBias);
        g1sum      = simde_mm256_add_epi16(g1sum, rv_batch);
        if(illegalMap[loc1])
          g1sum = simde_mm256_add_epi16(g1sum, iv1_batch);

        auto h1 = simde_mm256_max_epi16(g1sum, simde_mm256_mulhrs_epi16(g1sum, g1lr_w));
        simde_mm256_storeu_si256(h1mbias + 0 * 16,
                                 simde_mm256_mulhrs_epi16(h1, h1conv_w0));
        simde_mm256_storeu_si256(h1mbias + 1 * 16,
                                 simde_mm256_mulhrs_epi16(h1, h1conv_w1));
        simde_mm256_storeu_si256(h1mbias + 2 * 16,
                                 simde_mm256_mulhrs_epi16(h1, h1conv_w2));
        simde_mm256_storeu_si256(h1mbias + 3 * 16,
                                 simde_mm256_mulhrs_epi16(h1, h1conv_w3));
        simde_mm256_storeu_si256(h1mbias + 4 * 16,
                                 simde_mm256_mulhrs_epi16(h1, h1conv_w4));
        simde_mm256_storeu_si256(h1mbias + 5 * 16,
                                 simde_mm256_mulhrs_epi16(h1, h1conv_w5));
        simde_mm256_storeu_si256(h1mbias + 6 * 16,
                                 simde_mm256_mulhrs_epi16(h1, h1conv_w6));
        static_assert(featureHalfLen == 6, "h1conv group = featureHalfLen + 1");
      }
    }

    //-------------------------------------------------------------------------------------------------------------------------------------------------------------------
    int16_t h3[MaxBS*MaxBS][16]; //已经加上h3lr_b

    auto h1lr1_w = simde_mm256_loadu_si256(weights->h1lr1_w + addrBias);
    auto h1lr2_w = simde_mm256_loadu_si256(weights->h1lr2_w + addrBias);
    auto h1conv_b = simde_mm256_loadu_si256(weights->h1conv_b + addrBias);
    auto h3lr_b    = simde_mm256_loadu_si256(weights->h3lr_b + addrBias);
    auto iv2_batch = simde_mm256_loadu_si256(weights->illegalVector + groupSize + addrBias);//illegalVector

    for (NU_Loc locY = 0; locY < MaxBS; locY++) {
      for (NU_Loc locX = 0; locX < MaxBS; locX++) {
        NU_Loc loc1 = locY * MaxBS + locX;             //原始loc
        NU_Loc      loc2    = (locY + featureHalfLen) * (MaxBS + 2 * featureHalfLen) + locX + featureHalfLen;  // padding后的loc
        int16_t* h1mbias = buf.h1m + loc2 * (featureHalfLen + 1) * 16;

        auto h2sum = h3lr_b;
        if (illegalMap[loc1])
          h2sum = simde_mm256_add_epi16(h2sum, iv2_batch);

        const int dloc2s[4] = {1, MaxBS + 2 * featureHalfLen, MaxBS + 2 * featureHalfLen + 1, -MaxBS - 2 * featureHalfLen + 1};
        for (int dir=0;dir<4;dir++)
        {
          const int dloc2 = dloc2s[dir];  

          //把所有需要的全都load出来
          auto      g2    = simde_mm256_loadu_si256(buf.g2[loc1][dir] + addrBias);
          auto h1cm6  = simde_mm256_loadu_si256(h1mbias - 6 * 16 * ((featureHalfLen + 1) * dloc2 - 1));
          auto h1cm5  = simde_mm256_loadu_si256(h1mbias - 5 * 16 * ((featureHalfLen + 1) * dloc2 - 1));
          auto h1cm4  = simde_mm256_loadu_si256(h1mbias - 4 * 16 * ((featureHalfLen + 1) * dloc2 - 1));
          auto h1cm3  = simde_mm256_loadu_si256(h1mbias - 3 * 16 * ((featureHalfLen + 1) * dloc2 - 1));
          auto h1cm2  = simde_mm256_loadu_si256(h1mbias - 2 * 16 * ((featureHalfLen + 1) * dloc2 - 1));
          auto h1cm1  = simde_mm256_loadu_si256(h1mbias - 1 * 16 * ((featureHalfLen + 1) * dloc2 - 1));
          auto h1c0   = simde_mm256_loadu_si256(h1mbias);
          auto h1c1   = simde_mm256_loadu_si256(h1mbias + 1 * 16 * ((featureHalfLen + 1) * dloc2 + 1));
          auto h1c2   = simde_mm256_loadu_si256(h1mbias + 2 * 16 * ((featureHalfLen + 1) * dloc2 + 1));
          auto h1c3   = simde_mm256_loadu_si256(h1mbias + 3 * 16 * ((featureHalfLen + 1) * dloc2 + 1));
          auto h1c4   = simde_mm256_loadu_si256(h1mbias + 4 * 16 * ((featureHalfLen + 1) * dloc2 + 1));
          auto h1c5   = simde_mm256_loadu_si256(h1mbias + 5 * 16 * ((featureHalfLen + 1) * dloc2 + 1));
          auto h1c6   = simde_mm256_loadu_si256(h1mbias + 6 * 16 * ((featureHalfLen + 1) * dloc2 + 1));
          static_assert(featureHalfLen == 6, "h1conv group = featureHalfLen + 1");

          //13个h1c和h1conv_b全部相加，使用“二叉树”式加法
          h1cm6 = simde_mm256_adds_epi16(h1cm6, h1conv_b);
          h1cm4 = simde_mm256_adds_epi16(h1cm4, h1cm5);
          h1cm2 = simde_mm256_adds_epi16(h1cm2, h1cm3);
          h1c0  = simde_mm256_adds_epi16(h1c0, h1cm1);
          h1c2  = simde_mm256_adds_epi16(h1c2, h1c1);
          h1c4 = simde_mm256_adds_epi16(h1c4, h1c3);
          h1c6 = simde_mm256_adds_epi16(h1c6, h1c5);

          h1cm6 = simde_mm256_adds_epi16(h1cm6, h1cm4);
          h1cm6 = simde_mm256_adds_epi16(h1cm6, h1c6);
          h1cm2 = simde_mm256_adds_epi16(h1cm2, h1c0);
          h1c4 = simde_mm256_adds_epi16(h1c4, h1c2);


          auto h2 = simde_mm256_adds_epi16(h1cm6, h1cm2);
          h2      = simde_mm256_adds_epi16(h1c4, h2);
          static_assert(featureHalfLen == 6, "h1conv group = featureHalfLen + 1");

          //h1lr1
          h2 = simde_mm256_max_epi16(h2, simde_mm256_mulhrs_epi16(h2, h1lr1_w));
          //+g2
          h2 = simde_mm256_adds_epi16(h2, g2);
          //h1lr2
          h2 = simde_mm256_max_epi16(h2, simde_mm256_mulhrs_epi16(h2, h1lr2_w));

          
          h2sum = simde_mm256_adds_epi16(h2sum, h2); //h2sum=mean(h2)=(h2+h2+h2+h2)/4
        }
        //save h3
        simde_mm256_storeu_si256(h3[loc1],h2sum);
      }
    }

    
    //-------------------------------------------------------------------------------------------------------------------------------------------------------------------
    int16_t trunk1[(MaxBS+2) * (MaxBS+2)][16];//trunkconv2前的trunk，padding=1
    memset(trunk1, 0, sizeof(int16_t) * (MaxBS + 2) * (MaxBS + 2) * 16);
    //需要用到的权重
    auto h3lr_w  = simde_mm256_loadu_si256(weights->h3lr_w + addrBias);
    static_assert(trunkconv1GroupSize == 4, "现在的代码只支持trunkconv1GroupSize == 4");
    auto trunkconv1_b = simde_mm256_loadu_si256(weights->trunkconv1_b + addrBias);
    auto trunkconv1_w0 = simde_mm256_loadu_si256(weights->trunkconv1_w[0] + addrBias);
    auto trunkconv1_w1 = simde_mm256_loadu_si256(weights->trunkconv1_w[1] + addrBias);
    auto trunkconv1_w2 = simde_mm256_loadu_si256(weights->trunkconv1_w[2] + addrBias);
    auto trunkconv1_w3 = simde_mm256_loadu_si256(weights->trunkconv1_w[3] + addrBias);
    auto trunklr1_w = simde_mm256_loadu_si256(weights->trunklr1_w + addrBias);
    h3lr_b   = simde_mm256_loadu_si256(weights->h3lr_b + addrBias);

    for (NU_Loc locY = 0; locY < MaxBS; locY++) {
      for (NU_Loc locX = 0; locX < MaxBS; locX++) {
        NU_Loc loc1 = locY * MaxBS + locX;             //原始loc
        NU_Loc  loc2  = (locY + 1) * (MaxBS + 2) + locX + 1;  // padding后的loc
        auto trunk  = simde_mm256_loadu_si256(h3[loc1]);
        // h3lr
        trunk = simde_mm256_max_epi16(trunk, simde_mm256_mulhrs_epi16(trunk, h3lr_w));
        //trunkconv1
        trunk = simde_mm256_adds_epi16(
            trunkconv1_b,
            simde_mm256_adds_epi16(
                simde_mm256_adds_epi16(
                    simde_mm256_mulhrs_epi16(
                        simde_mm256_permute4x64_epi64(trunk, 0b00000000),
                        trunkconv1_w0),
                    simde_mm256_mulhrs_epi16(
                        simde_mm256_permute4x64_epi64(trunk, 0b01010101),
                        trunkconv1_w1)),
                simde_mm256_adds_epi16(
                    simde_mm256_mulhrs_epi16(
                        simde_mm256_permute4x64_epi64(trunk, 0b10101010),
                        trunkconv1_w2),
                    simde_mm256_mulhrs_epi16(
                        simde_mm256_permute4x64_epi64(trunk, 0b11111111),
                        trunkconv1_w3))));

        // trunklr1
        trunk = simde_mm256_max_epi16(trunk, simde_mm256_mulhrs_epi16(trunk, trunklr1_w));

        //save
        simde_mm256_storeu_si256(trunk1[loc2], trunk);
      }
    }

    //-------------------------------------------------------------------------------------------------------------------------------------------------------------------
    
    //需要用到的权重
    auto trunkconv2_w0 = simde_mm256_loadu_si256(weights->trunkconv2_w[0] + addrBias);
    auto trunkconv2_w1 = simde_mm256_loadu_si256(weights->trunkconv2_w[1] + addrBias);
    auto trunkconv2_w2 = simde_mm256_loadu_si256(weights->trunkconv2_w[2] + addrBias);
    auto trunklr2_b = simde_mm256_loadu_si256(weights->trunklr2_b + batch * 16);
    auto trunklr2_w = simde_mm256_loadu_si256(weights->trunklr2_w + batch * 16);

    auto vsum0 = simde_mm256_setzero_si256();
    auto vsum1 = simde_mm256_setzero_si256();
    for (NU_Loc locY = 0; locY < MaxBS; locY++) {
      for (NU_Loc locX = 0; locX < MaxBS; locX++) {
        NU_Loc  loc1  = locY * MaxBS + locX;            //原始loc
        NU_Loc  loc2   = (locY + 1) * (MaxBS + 2) + locX + 1;  // padding后的loc
        auto trunka = simde_mm256_adds_epi16(
            simde_mm256_adds_epi16(simde_mm256_loadu_si256(trunk1[loc2 - (MaxBS + 2)]),
                                   simde_mm256_loadu_si256(trunk1[loc2 + (MaxBS + 2)])),
            simde_mm256_adds_epi16(simde_mm256_loadu_si256(trunk1[loc2 - 1]),
                                   simde_mm256_loadu_si256(trunk1[loc2 + 1])));
        auto trunkb = simde_mm256_adds_epi16(
            simde_mm256_adds_epi16(simde_mm256_loadu_si256(trunk1[loc2 - (MaxBS + 2) - 1]),
                                   simde_mm256_loadu_si256(trunk1[loc2 - (MaxBS + 2) + 1])),
            simde_mm256_adds_epi16(simde_mm256_loadu_si256(trunk1[loc2 + (MaxBS + 2) - 1]),
                                   simde_mm256_loadu_si256(trunk1[loc2 + (MaxBS + 2) + 1])));
        auto trunk = simde_mm256_loadu_si256(trunk1[loc2]);

        trunk = simde_mm256_mulhrs_epi16(trunk, trunkconv2_w0);
        trunk = simde_mm256_adds_epi16(simde_mm256_mulhrs_epi16(trunka, trunkconv2_w1), trunk);
        trunk = simde_mm256_adds_epi16(simde_mm256_mulhrs_epi16(trunkb, trunkconv2_w2), trunk);

        trunk = simde_mm256_adds_epi16(trunk, trunklr2_b);
        trunk = simde_mm256_max_epi16(trunk, simde_mm256_mulhrs_epi16(trunk, trunklr2_w));

        vsum0 = simde_mm256_add_epi32(
          vsum0,
          simde_mm256_cvtepi16_epi32(simde_mm256_extractf128_si256(trunk, 0)));
        vsum1 = simde_mm256_add_epi32(
          vsum1,
          simde_mm256_cvtepi16_epi32(simde_mm256_extractf128_si256(trunk, 1)));

        // save
        simde_mm256_storeu_si256(buf.trunk[loc1]+addrBias, trunk);
      }
    }
    simde_mm256_storeu_ps(vsum + batch * 16, simde_mm256_cvtepi32_ps(vsum0));
    simde_mm256_storeu_ps(vsum + batch * 16 + 8, simde_mm256_cvtepi32_ps(vsum1));

  }


  //scale, valuelr
  auto scale = simde_mm256_set1_ps(weights->scale_beforemlpInv / (MaxBS * MaxBS));
  for (int batch32 = 0; batch32 < groupBatch * 2; batch32++) {
    auto valuelr_b = simde_mm256_loadu_ps(weights->valuelr_b + batch32 * 8);
    auto valuelr_w = simde_mm256_loadu_ps(weights->valuelr_w + batch32 * 8);
    auto v = simde_mm256_loadu_ps(vsum + batch32 * 8);
    v = simde_mm256_mul_ps(v, scale);
    v = simde_mm256_add_ps(v, valuelr_b);
    v = simde_mm256_max_ps(v, simde_mm256_mul_ps(v, valuelr_w));
    simde_mm256_storeu_ps(vsum + batch32 * 8, v);

  }

  // linear 1
  float layer1[mlpChannel];
  for (int i = 0; i < mlpBatch32; i++) {
    auto sum = simde_mm256_loadu_ps(weights->mlp_b1 + i * 8);
    for (int j = 0; j < groupSize; j++) {
      auto x = simde_mm256_set1_ps(vsum[j]);
      auto w = simde_mm256_loadu_ps(weights->mlp_w1[j] + i * 8);
      sum = simde_mm256_fmadd_ps(w, x, sum);
    }
    sum = simde_mm256_max_ps(simde_mm256_setzero_ps(), sum);  // relu
    simde_mm256_storeu_ps(layer1 + i * 8, sum);
  }

  // linear 2
  float layer2[mlpChannel];
  for (int i = 0; i < mlpBatch32; i++) {
    auto sum = simde_mm256_loadu_ps(weights->mlp_b2 + i * 8);
    for (int j = 0; j < mlpChannel; j++) {
      auto x = simde_mm256_set1_ps(layer1[j]);
      auto w = simde_mm256_loadu_ps(weights->mlp_w2[j] + i * 8);
      sum = simde_mm256_fmadd_ps(w, x, sum);
    }
    sum = simde_mm256_max_ps(simde_mm256_setzero_ps(), sum);  // relu
    simde_mm256_storeu_ps(layer2 + i * 8, sum);
  }

  // linear 3
  float layer3[mlpChannel];
  for (int i = 0; i < mlpBatch32; i++) {
    auto sum = simde_mm256_loadu_ps(weights->mlp_b3 + i * 8);
    for (int j = 0; j < mlpChannel; j++) {
      auto x = simde_mm256_set1_ps(layer2[j]);
      auto w = simde_mm256_loadu_ps(weights->mlp_w3[j] + i * 8);
      sum = simde_mm256_fmadd_ps(w, x, sum);
    }
    sum = simde_mm256_max_ps(simde_mm256_setzero_ps(), sum);  // relu
    sum = simde_mm256_add_ps(sum, simde_mm256_loadu_ps(layer1 + i * 8));//resnet connection
    simde_mm256_storeu_ps(layer3 + i * 8, sum);
  }

  // linear 4
  for (int i = 0; i < mlpBatch32; i++) {
    auto sum = simde_mm256_loadu_ps(weights->mlp_b4 + i * 8);
    for (int j = 0; j < mlpChannel; j++) {
      auto x = simde_mm256_set1_ps(layer3[j]);
      auto w = simde_mm256_loadu_ps(weights->mlp_w4[j] + i * 8);
      sum = simde_mm256_fmadd_ps(w, x, sum);
    }
    sum = simde_mm256_max_ps(simde_mm256_setzero_ps(), sum);  // relu
    simde_mm256_storeu_ps(buf.mlp_layer4 + i * 8, sum);
  }

  // final linear(value head)
  auto v = simde_mm256_loadu_ps(weights->mlpfinal_b);
  for (int inc = 0; inc < mlpChannel; inc++) {
    auto x = simde_mm256_set1_ps(buf.mlp_layer4[inc]);
    auto w = simde_mm256_loadu_ps(weights->mlpfinal_w[inc]);
    v = simde_mm256_fmadd_ps(w, x, v);
  }
  simde_mm256_storeu_ps(buf.mlp_value, v);

  buf.trunkUpToDate = true;
  return;

}


void ModelBuf::emptyboard(const ModelWeight* weights)
{
  trunkUpToDate = false;

  // shape table
  {
    for (int i = 0; i < 4; i++)
      for (int j = 0; j < MaxBS * MaxBS; j++)
        shapeTable[j][i] = 0;

    //以下不可交换次序，因为存在覆盖

    //正方向的墙（右墙，下墙，右下墙，右上墙）
    const int fhl = featureHalfLen;
    const int fhlp1 = featureHalfLen + 1;
    for (int thick = 1; thick <= fhl; thick++) {
      for (int i = 0; i < MaxBS; i++) {
        int c = 0;
        for (int j = 0; j < thick; j++)
          c += pow3[featureLen - j];
        shapeTable[(MaxBS - fhlp1 + thick) + i * MaxBS][0] = c;  //右墙
        shapeTable[i + (MaxBS - fhlp1 + thick) * MaxBS][1] = c;  //下墙
        shapeTable[(MaxBS - fhlp1 + thick) + i * MaxBS][2] = c;  //右下墙靠右
        shapeTable[i + (MaxBS - fhlp1 + thick) * MaxBS][2] = c;  //右下墙靠下
        shapeTable[(MaxBS - fhlp1 + thick) + i * MaxBS][3] = c;  //右上墙靠右
        shapeTable[i + (fhlp1 - 1 - thick) * MaxBS][3]  = c;  //右下墙靠上
      }
    }

    //负方向的墙（左墙，上墙，左上墙，左下墙）

    //厚度1
    for (int thick = 1; thick <= fhl; thick++) {
      for (int i = 0; i < MaxBS; i++) {
        int c = 2 * pow3[featureLen];  // 3进制2000000000
        for (int j = 0; j < thick - 1; j++)
          c += pow3[j];
        shapeTable[(fhlp1 - 1 - thick) + i * MaxBS][0]  = c;  //左墙
        shapeTable[i + (fhlp1 - 1 - thick) * MaxBS][1]  = c;  //上墙
        shapeTable[(fhlp1 - 1 - thick) + i * MaxBS][2]  = c;  //左上墙靠左
        shapeTable[i + (fhlp1 - 1 - thick) * MaxBS][2]  = c;  //左上墙靠上
        shapeTable[(fhlp1 - 1 - thick) + i * MaxBS][3]  = c;  //左下墙靠左
        shapeTable[i + (MaxBS - fhlp1 + thick) * MaxBS][3] = c;  //左下墙靠下
      }
    }

    //两边都有墙

    for (int a = 1; a <= fhl; a++)    //正方向墙厚
      for (int b = 1; b <= fhl; b++)  //负方向墙厚
      {
        int c = 3 * pow3[featureLen];
        for (int i = 0; i < a - 1; i++)
          c += pow3[featureLen - 1 - i];
        for (int i = 0; i < b - 1; i++)
          c += pow3[i];
        shapeTable[(MaxBS - fhlp1 + a) + (fhl - b) * MaxBS][2]      = c;  //右上角
        shapeTable[(MaxBS - fhlp1 + a) * MaxBS + (fhl - b)][2]      = c;  //左下角
        shapeTable[(fhl - b) + (fhl - a) * MaxBS][3]           = c;  //左上角
        shapeTable[(MaxBS - fhlp1 + a) + (MaxBS - fhlp1 + b) * MaxBS][3] = c;  //右下角
      }
  }

  //g1 and g2
  for (NU_Loc loc = 0; loc < MaxBS*MaxBS; loc++) {

    for (int i = 0; i < groupBatch; i++) {
      auto g1sum_ = simde_mm256_setzero_si256();
      for (int dir = 0; dir < 4; dir++) {
        // g2 update
        auto neww = simde_mm256_loadu_si256(weights->mapping[shapeTable[loc][dir]] + i * 16 + groupSize);
        simde_mm256_storeu_si256(g2[loc][dir] + i * 16, neww);
        // g1 update
        auto g1 = simde_mm256_loadu_si256(weights->mapping[shapeTable[loc][dir]] + i * 16);
        g1sum_  = simde_mm256_add_epi16(g1sum_, g1);
      }

      simde_mm256_storeu_si256(g1sum[loc] + i * 16, g1sum_);
    }
  }
}

Eva_nnuev2::Eva_nnuev2(const NNUEV2::ModelWeight* w) :weights(w){
  clear();
}

void Eva_nnuev2::clear() {
  for (int i = 0; i < MaxBS * MaxBS; i++)
    board[i] = C_EMPTY;
  buf.emptyboard(weights);
}

void Eva_nnuev2::recalculate()
{
  Color boardCopy[MaxBS * MaxBS];
  memcpy(boardCopy, board, MaxBS * MaxBS * sizeof(Color));
  clear();
  for (NU_Loc i = 0; i < MaxBS * MaxBS; ++i) {
    if (boardCopy[i] != C_EMPTY)
      play(boardCopy[i], i);
  }
}

void Eva_nnuev2::play(Color color, NU_Loc loc)
{
  if (loc < 0 || loc >= MaxBS * MaxBS)return;
  board[loc] = color;
  buf.update(C_EMPTY, color, loc, weights);
}

ValueType Eva_nnuev2::evaluateFull(const float *gf, const bool* illegalMap, PolicyType *policy)
{
  if (policy != nullptr) {
    evaluatePolicy(gf, illegalMap, policy);
  }
  return evaluateValue(gf, illegalMap);
}

void Eva_nnuev2::evaluatePolicy(const float *gf, const bool* illegalMap, PolicyType *policy)
{
  if (policy == NULL)
    return;
  if (!buf.trunkUpToDate)
    calculateTrunk(gf, illegalMap);

  // mlp_p linear 
  float p_w_float[groupSize];
  int16_t p_w[groupSize];
  for (int i = 0; i < groupBatch32; i++) {
    auto sum = simde_mm256_loadu_ps(weights->mlp_p_b + i * 8);
    for (int j = 0; j < mlpChannel; j++) {
      auto x = simde_mm256_set1_ps(buf.mlp_layer4[j]);
      auto w = simde_mm256_loadu_ps(weights->mlp_p_w[j] + i * 8);
      sum = simde_mm256_fmadd_ps(w, x, sum);
    }
    auto prelu_w = simde_mm256_loadu_ps(weights->mlp_plr_w + i * 8);
    sum = simde_mm256_max_ps(simde_mm256_mul_ps(sum, prelu_w), sum);  // prelu
    simde_mm256_storeu_ps(p_w_float + i * 8, sum);
  }

  //too lazy to write avx2 code
  const float clipBound = 32768 * 0.99;
  for (int i = 0; i < groupSize; i++) {
    float t = p_w_float[i];
    t = t > clipBound ? clipBound : t;
    t = t < -clipBound ? -clipBound : t;
    p_w[i] = int16_t(t);
  }


  for (NU_Loc loc = 0; loc < MaxBS * MaxBS; loc++) {
    auto psum = simde_mm256_setzero_si256();//int32
    for (int batch = 0; batch < groupBatch; batch++) {

      //load
      auto t = simde_mm256_loadu_si256(buf.trunk[loc] + batch * 16);
      auto policy_linear_w = simde_mm256_loadu_si256(p_w + batch * 16);


      //policy linear
      t          = simde_mm256_madd_epi16(t, policy_linear_w); 
      psum       = simde_mm256_add_epi32(t, psum);

    }

    psum = simde_mm256_hadd_epi32(psum, psum);
    psum = simde_mm256_hadd_epi32(psum, psum);

    auto  p = simde_mm256_extract_epi32(psum, 0) + simde_mm256_extract_epi32(psum, 4);
    policy[loc] = p * weights->scale_beforemlpInv * policyQuantFactor / 32768;
    if (illegalMap[loc])
      policy[loc] -= 100.0 * policyQuantFactor;
  }

  policy[MaxBS * MaxBS] = buf.mlp_value[3] * policyQuantFactor;//pass
}

ValueType Eva_nnuev2::evaluateValue(const float *gf, const bool* illegalMap)
{
  if (!buf.trunkUpToDate)
    calculateTrunk(gf, illegalMap);


  return ValueType(buf.mlp_value[0], buf.mlp_value[1], buf.mlp_value[2]);
}

void Eva_nnuev2::undo(NU_Loc loc)
{
  if (loc < 0 || loc >= MaxBS * MaxBS)return;
  buf.update(board[loc], C_EMPTY, loc, weights);
  board[loc] = C_EMPTY;
}

void Eva_nnuev2::debug_print()
{
  using namespace std;
  NU_Loc loc = MakeLoc(0, 0);
  PolicyType p[MaxBS * MaxBS];
  float gf[NNUEV2::globalFeatureNum] = { 0 };
  bool illegalMap[MaxBS*MaxBS] = { false };
  auto       v = evaluateFull(gf, illegalMap, p);
  cout << "value: win=" << v.win << " loss=" << v.loss << " draw=" << v.draw << endl;
  //for (int i = 48; i < groupSize; i++)
  //  cout << buf.g1sum[loc][i] << "|" << buf.g2[loc][3][i] << "|" << buf.trunk[loc][i] << " ";


  cout << "policy: " <<  endl;
  for (int y = 0; y < MaxBS; y++) {
    for (int x = 0; x < MaxBS; x++)
      cout << p[y*MaxBS+x] << "\t";
    cout << endl;
  }
  cout << endl;
  /*
  cout << "mapsum";
  for (int i = 0; i < gr; i++)
    cout << buf.mapsum[loc][i] << " ";
  cout << endl;
  cout << "mapafterlr";
  for (int i = 0; i < mix6::featureNum; i++)
    cout << buf.mapAfterLR[loc][i] << " ";
  cout << endl;
  cout << "policyafterconv";
  for (int i = 0; i < mix6::policyNum; i++)
    cout << buf.policyAfterConv[loc][i] << " ";
  cout << endl;
  cout << "valueavg";
  for (int i = 0; i < mix6::valueNum; i++)
    cout << float(buf.valueSumBoard[i]) / (MaxBS * MaxBS) << " ";
  cout << endl;*/
}

bool ModelWeight::loadParam(std::string filepath) {
  using namespace std::filesystem;
  path ext = path(filepath).extension();
  if(ext.string() == ".bin") {
    std::ifstream cacheStream(path(filepath), std::ios::binary);
    cacheStream.read(reinterpret_cast<char*>(this), sizeof(ModelWeight));
    if(cacheStream.good()) {
      return true;
    } else
      return false;
  }

  path cachePath = path(filepath).replace_extension("bin");
  // Read parameter cache if exists
  if(exists(cachePath)) {
    std::ifstream cacheStream(cachePath, std::ios::binary);
    cacheStream.read(reinterpret_cast<char*>(this), sizeof(ModelWeight));
    if(cacheStream.good()) {
      return true;
    }
    else
    {
      std::cout << "Bad NNUE bin file: " << cachePath << std::endl;
      return false;
    }
  }

  bool suc = loadParamTxt(filepath);
  if(suc) {
    std::ofstream cacheStream(cachePath, std::ios::binary);
    cacheStream.write(reinterpret_cast<char*>(this), sizeof(ModelWeight));
  }
  else {
    std::cout << "Bad NNUE txt file: " << cachePath << std::endl;
    return false;
  }
  return suc;
}

bool ModelWeight::loadParamTxt(std::string filename)
{
  using namespace std;
  ifstream fs(filename);

  // clear map
  for (int i = 0; i < shapeNum; i++) {
    for (int j = 0; j < featureNum; j++)
      mapping[i][j] = IllegalShapeFeature;
  }

  string modelname;
  fs >> modelname;
  if (modelname != "v2") {
    cout << "Wrong model type:" << modelname << endl;
    return false;
  }

  int param;
  fs >> param;
  if (param != groupSize) {
    cout << "Wrong group size:" << param << endl;
    return false;
  }
  fs >> param;
  if (param != mlpChannel) {
    cout << "Wrong mlp channel:" << param << endl;
    return false;
  }



  string varname;

  // mapping
  fs >> varname;
  if (varname != "mapping") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  int shapeN;
  fs >> shapeN;
  for (int i = 0; i < shapeN; i++) {
    int shapeID;
    fs >> shapeID;
    for (int j = 0; j < featureNum; j++)
      fs >> mapping[shapeID][j];
  }

  //gfvector_w
  fs >> varname;
  if (varname != "gfvector_w") {
      cout << "Wrong parameter name:" << varname << endl;
      return false;
  }
  for (int j = 0; j < globalFeatureNum; j++)
      for (int i = 0; i < groupSize; i++)
          fs >> gfmlp_w[j][i];

  // gfvector_b1
  fs >> varname;
  if (varname != "gfvector_b") {
      cout << "Wrong parameter name:" << varname << endl;
      return false;
  }
  for (int i = 0; i < groupSize; i++)
      fs >> gfmlp_b[i];


  // illegalvector
  fs >> varname;
  if (varname != "illegalVector") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int i = 0; i < featureNum; i++)
    fs >> illegalVector[i];



  // g1lr_w
  fs >> varname;
  if (varname != "g1lr_w") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int i = 0; i < groupSize; i++)
    fs >> g1lr_w[i];

  // h1conv_w
  fs >> varname;
  if (varname != "h1conv_w") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int j = 0; j < featureHalfLen + 1; j++)
    for (int i = 0; i < groupSize; i++)
      fs >> h1conv_w[j][i];

  // h1conv_b
  fs >> varname;
  if (varname != "h1conv_b") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int i = 0; i < groupSize; i++)
    fs >> h1conv_b[i];

  // h1lr1_w
  fs >> varname;
  if (varname != "h1lr1_w") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int i = 0; i < groupSize; i++)
    fs >> h1lr1_w[i];

  // h1lr2_w
  fs >> varname;
  if (varname != "h1lr2_w") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int i = 0; i < groupSize; i++)
    fs >> h1lr2_w[i];

  // h3lr_w
  fs >> varname;
  if (varname != "h3lr_w") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int i = 0; i < groupSize; i++)
    fs >> h3lr_w[i];

  // h3lr_b
  fs >> varname;
  if (varname != "h3lr_b") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int i = 0; i < groupSize; i++)
    fs >> h3lr_b[i];

  // trunkconv1_w
  fs >> varname;
  if (varname != "trunkconv1_w") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int j = 0; j < trunkconv1GroupSize; j++)
    for (int i = 0; i < groupSize; i++)
        fs >> trunkconv1_w[j][i];

  // trunkconv1_b
  fs >> varname;
  if (varname != "trunkconv1_b") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int i = 0; i < groupSize; i++)
    fs >> trunkconv1_b[i];

  // trunklr1_w
  fs >> varname;
  if (varname != "trunklr1_w") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int i = 0; i < groupSize; i++)
    fs >> trunklr1_w[i];

  // trunkconv2_w
  fs >> varname;
  if (varname != "trunkconv2_w") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int j = 0; j < 3; j++)
    for (int i = 0; i < groupSize; i++)
        fs >> trunkconv2_w[j][i];

  // trunklr2p_w
  fs >> varname;
  if (varname != "trunklr2_w") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int i = 0; i < groupSize; i++)
    fs >> trunklr2_w[i];

  // trunklr2p_b
  fs >> varname;
  if (varname != "trunklr2_b") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int i = 0; i < groupSize; i++)
    fs >> trunklr2_b[i];


  // scale_beforemlpInv
  fs >> varname;
  if (varname != "scale_beforemlpInv") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  fs >> scale_beforemlpInv;

  // valuelr_w
  fs >> varname;
  if (varname != "valuelr_w") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int i = 0; i < groupSize; i++)
    fs >> valuelr_w[i];

  // valuelr_b
  fs >> varname;
  if (varname != "valuelr_b") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int i = 0; i < groupSize; i++)
    fs >> valuelr_b[i];

  // mlp_w1
  fs >> varname;
  if (varname != "mlp_w1") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int j = 0; j < groupSize; j++)
    for (int i = 0; i < mlpChannel; i++)
        fs >> mlp_w1[j][i];

  // mlp_b1
  fs >> varname;
  if (varname != "mlp_b1") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int i = 0; i < mlpChannel; i++)
    fs >> mlp_b1[i];

  // mlp_w2
  fs >> varname;
  if (varname != "mlp_w2") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int j = 0; j < mlpChannel; j++)
    for (int i = 0; i < mlpChannel; i++)
      fs >> mlp_w2[j][i];

  // mlp_b2
  fs >> varname;
  if (varname != "mlp_b2") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int i = 0; i < mlpChannel; i++)
    fs >> mlp_b2[i];

  // mlp_w3
  fs >> varname;
  if (varname != "mlp_w3") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int j = 0; j < mlpChannel; j++)
    for (int i = 0; i < mlpChannel; i++)
      fs >> mlp_w3[j][i];

  // mlp_b3
  fs >> varname;
  if (varname != "mlp_b3") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int i = 0; i < mlpChannel; i++)
    fs >> mlp_b3[i];

  // mlp_w4
  fs >> varname;
  if (varname != "mlp_w4") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int j = 0; j < mlpChannel; j++)
    for (int i = 0; i < mlpChannel; i++)
      fs >> mlp_w4[j][i];

  // mlp_b4
  fs >> varname;
  if (varname != "mlp_b4") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int i = 0; i < mlpChannel; i++)
    fs >> mlp_b4[i];

  // mlpfinal_w
  fs >> varname;
  if (varname != "mlpfinal_w") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int j = 0; j < mlpChannel; j++)
    for (int i = 0; i < 4; i++)
      fs >> mlpfinal_w[j][i];

  // mlpfinal_b
  fs >> varname;
  if (varname != "mlpfinal_b") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int i = 0; i < 4; i++)
    fs >> mlpfinal_b[i];

  for (int i = 0; i < 4; i++) {
    mlpfinal_w_for_safety[i] = 0;
    mlpfinal_b_for_safety[i] = 0;
  }

  // mlp_p_w
  fs >> varname;
  if (varname != "mlp_p_w") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int j = 0; j < mlpChannel; j++)
    for (int i = 0; i < groupSize; i++)
      fs >> mlp_p_w[j][i];

  // mlp_p_b
  fs >> varname;
  if (varname != "mlp_p_b") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int i = 0; i < groupSize; i++)
    fs >> mlp_p_b[i];

  // mlp_plr_w
  fs >> varname;
  if (varname != "mlp_plr_w") {
    cout << "Wrong parameter name:" << varname << endl;
    return false;
  }
  for (int i = 0; i < groupSize; i++)
    fs >> mlp_plr_w[i];



  return true;
}