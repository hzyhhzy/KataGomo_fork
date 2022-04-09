***2022.1 Engine based on Katago TensorRT+GraphSearch***   

***Updated on 2022.4.9***   

Gomoku/Renju: "GomDevVCN" branch   
https://github.com/hzyhhzy/KataGo/tree/GomDevVCN      

TODO:   
先做改动小的，再做改动大的   

完全没改的分支：Go2022   
| 棋种                                                   | branch         | 状态         | 备注                               |
| :----------------------------------------------------- | :------------- | :----------- | :--------------------------------- |
| 1.各种变种围棋（保留中国规则，删掉日韩规则）           | CNrule2022     | -            | 基础分支                           |
| 1.1 加权点目                                           | weightGo       | finished     |
| 1.2 某个子不能死                                       |                | aborted      | 不做了，因为需要大量死活题作为开局 |
| 1.3.1 Hex 棋盘的围棋                                   | HexGo          | aborted      | 懒得做了                           |
| 1.3.2 Hex 棋盘的吃子棋                                 | HexCaptureGo   | finished     |
| 1.4 落子没气先提自己                                   | firstCaptureMe | finished     |
| 1.5 一子千金                                           | yiziqianjin    | finished     |
|                                                        |                |
| 2.需要提子但不需要点目的                               | CaptureGo2022  | -            | 基础分支                           |
| 2.1 吃子棋                                             | CaptureGo2022  | finished     |
| 2.2 反吃子棋（不围棋）                                 | CaptureGo2022  | finished     |
| 2.3 活一块就算赢(死活对局)                             | lifego2        | finished     |
| 2.4 黑棋活一块就赢，但是不能被吃子                     | aliveWin       | finished     |
| 2.5 吃子 n 子棋                                        |                | aborted      | 以前做过所以不做了                 |
| 2.6 白能吃黑，黑吃不掉白，黑棋最多多少手不被吃         | EscapeGo       | finished     |
| 2.7 谁先没地方下谁输                                   | yiziqianjin    | finished     | 视为特殊的一子千金                 |
|                                                        |
| 3.不需要提子，黑白交替落子的棋                         | Gom2022base    | -            | 基础分支，不跑训练                 |
| 3.0 不打算做的                                         |                |
| 3.0.2 重力四子棋                                       |                | aborted      | 以前做过，训练了很长时间           |
| 3.1 五子棋系列(各种规则)                               |                | -            |
| 3.1.0 主线(无禁,有禁,无禁六不胜)                       | GomDevVCN      | **training** | 100b256f，以后会跑分布式           |
| 3.1.2 禁点五子棋                                       | GomBanLoc      | finished     |
| 3.2 连五的个数                                         | fivecount      | finished     |
| 3.3 反 n 子棋                                          | AntiGomoku     | finished     |
| 3.4 Hex                                                | Hex2022        | finished     |
| 3.4.1 反 Hex                                           | Hex2022        | finished     |
| 3.5 等差数列 6 子棋                                    | EquDifGomoku   | finished     |
| 3.6 Angels and Devils game                             | AngelProblem   | finished     |
| 3.7 每一步必须在上一步的附近某些位置，满足某些条件获胜 |                | -            |
| 3.7.1 一种特殊四子棋                                   | con4type1      | finished     |
| 3.8 Reversi(奥赛罗，翻转棋)，反 Reversi                | Reversi2022    | finished     | 为了在botzone上打榜，还是做了      |
|                                                        |
| 4.一次走两步的棋，或者挪子的棋                         | Amazons        | -            | 把 Amazons 分支作为基础分支        |
| 4.0 国象，中象                                         |                | aborted      | 别人做过，且效果不好，所以不做     |
| 4.1 六子棋(Connect6)                                   | Connect6       | **training** |
| 4.2 中国跳棋                                           | tiaoqi         | finished     |
| 4.3 Amazons(亚马逊棋)                                  | Amazons        | finished     |
| 4.4 Breakthrough                                       | breakthrough   | finished     |
| 4.5 Ataxx(同化棋)                                      | Ataxx          | finished     |
| 4.6 各种极简象棋变种                                   |                | todo         |
|                                                        |
| 5.单人游戏                                             | -              |              |
| 5.1 消消乐                                             | xiaoxiaole     | todo         |
|                                                        |
| 6.完全信息，但是有随机性                               |                | todo         |




***Updated on 2021.4.5***
Now ***Gomoku/Renju*** code is uploading in "Gomoku" branch.




----------- 2020.7.26 ----------

Now there are gomoku(freestyle,standard,renju,caro), reversi, connect6, breakthrough, hex, "four in a row", Chinese checkers, and many board games few people play.

Now it's the strongest program on this games as I know: 
all kinds of gomoku(compared with Embryo)
connect6(compared with gzero)
hex(compared with gzero and)
Chinese checkers(compared with Shangxin Tiaoqi)

