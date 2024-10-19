# Recent progresses (Latest update: 2024.10.20)

| Branch              | Introduction                 | Stage        | Notes                              |
| :--------------- | :---------------------- | :----------- | :--------------------------------- |
| Gom2024   | Engine for Gomoku and Renju       | **Paused**     | Sponsored by Zhizi, will continue training months later  |
| ConnectSix2024   | Towards solving (proving black win) 19x19 Connect Six  | **Training**     | Will finish and release in ~ 10 days  |
| LifeGo2024   | Life-death Go  | **Training**     |   Will finish and release in ~ 5 days |
| HexGo2024   | Go on Hex board  | **Paused**     | Will finish and release in ~ 5 days |
| Quoridor2024   | Quoridor  | Finished     | Someone has already trained an AI months ago, and found first player win  |
| UltimateTictactoe2024   | Ultimate Tic-tac-toe  | Finished     | See the Release page |
| DawsonChess   | 2D Dawson-chess and variants (Reverse Gomoku)  | Finished     | Chaotic game |
| ConnectFour2024   | ConnectFour  | Finished     | See the Release page |
| CaptureGo2024   | Capture-Go  | Finished     | Will release in 2024.12 |
| Hex2024Maxmove   | Estimate the length of optimal Hex game      | Finished     | See the Release page  |
| Hex2024   | Hex  | Finished     | https://github.com/hzyhhzy/KataGo/releases/tag/Hex_20240812 |
| Caro2024   | Caro is a modified Gomoku  | Finished     | https://github.com/hzyhhzy/KataGo/releases/tag/Caro_20240804 |
| ProveSevenInARow   | See the Readme in the branch  | Finished     | https://mathoverflow.net/questions/471302/can-gomokufive-in-a-row-draw-on-an-infinite-board-what-about-other-m-n-k-game |
| HexTemplate   | Some special openings for Hex | Finished     | https://mathoverflow.net/questions/470376/connection-properties-of-a-single-stone-on-an-infinite-hex-board |
| DotsAndBoxes  |  - | Finished     | Not released yet |



-------------------------------
# Brief introduction of recent branches (Latest update: 2024.8.21)
## Basic branches for fast modification to all kinds of games
### BWN2024
"Black-white No-score 2024"
For games which black and white plays alternately
### Movestone2024
For board games with multi-stage moves or irregular move order
For example: Chess(regard one move as two stages: choose and place), Amazons(3 stages), Ataxx(sometimes 1 stage, some times 2 stages), ConnectSix, ...

## Gomoku (Freestyle or Standard) and Renju
### Gom2024
Latest KataGo engine for Gomoku and Renju. Also called "KataGomo" and "KataGomoku".     
Recently a b28c512nbt net is training under the sponsorship of Zhizi(智子)(www.zhizigo.cn www.zhizigomoku.cn), which provides online paid KataGo services.   
The training started in 2024.5 and will finish in 2024.11. ~30\*RTX4090 for 10 hours every day. The strongest net is only available on Zhizi.      

### Gom2024NNUE
An experiment of combining NNUE and NN. (not very success)
Gomoku-NNUE: A fast and small network with special structure for gomoku.      
Gomoku-NNUE is firstly created by me (https://github.com/hzyhhzy/gomoku_nnue/tree/multiRules) and improved by dblue and used in Rapfi(https://github.com/dhbloo/rapfi)     
Input the result of a small MCTS (~300 nodes) of NNUE to the big katago net.   
Result:  The katago net become "as strong as NNUE+MCTS" in a very short time, however further progress is very slow.
If use transfer-training from a trained katago net, it become slightly stronger than the origin net with same nodes, but not worth its large CPU cost.

### GomNNUEnested
A branch to test MCTS of NNUE.
Regarding the result of a small MCTS (~300 nodes) of NNUE as a "meta-node", then apply MCTS to meta-nodes.
Katago NN is not used here. Only to use katago's MCTS


## Hex and related problems
https://en.wikipedia.org/wiki/Hex_(board_game)   

### Hex2024Maxmove
Latest KataHex code
Hex2024 with move limit settings, to estimate the length of the optimal game of Hex board on N\*N board   
Result: f(N) ~ 0.6 \* N^1.9   
https://mathoverflow.net/questions/302821/length-of-optimal-play-in-hex-as-a-function-of-size/477037#477037

### Hex2024
Latest release of Katago for Hex game (KataHex)
https://github.com/hzyhhzy/KataGo/releases/tag/Hex_20240812   

### HexTemplate
A special engine for "edge template". Special network is needed.   
This branch is to estimate the min width of 8~11th row "edge template" of Hex     
Result: f(n)>=9\*n-46 for n>=8, and probably f(n)=9\*n-46 for n<=11   
https://www.hexwiki.net/index.php/Open_problems_about_edge_templates   

### HexTemplateSolver
A special engine for single-stone escape problems (HexTemplate branch is more specific for "edge template"). Special network is needed.    
Some analysis and results: https://mathoverflow.net/questions/470376/connection-properties-of-a-single-stone-on-an-infinite-hex-board   


## Other Gomoku-like Games
Including Caro, ConnectSix, and maybe some math problems

### ConnectSix2024
KataGo for Connect Six
The 2022 ver has shown that black has great advantage on 21x21 or larger boards, but 19x19 was unknown.
Recently @eeoo has found that black has advantage on 19x19, so I started a new run in 2024.8.
I decide to strictly prove that black wins in Connect Six on 19x19 boards, collaborating with dblue using his ConnectSix Rapfi.

### Caro2024
Caro is a modified Gomoku rule, the only difference is that fives with two terminals blocked are not a win (OXXXXXO is not a win)   
Latest release: https://github.com/hzyhhzy/KataGo/releases/tag/Caro_20240804

### ProveSevenInARow
We all know black wins in 5-in-a-row(Gomoku) on an infinite board. And more than 5 in a row is very difficult which means easy to draw.   
Although it's very easy even for a beginner to draw in 6-in-a-row game, 7-in-a-row has not been proven to be a draw in infinite boards and remains to be an open problem for decades, showing a big gap between "theory" and "practice".   
https://arxiv.org/abs/2107.05363 This article shows a method to reduce the infinite board to 4\*n boards. If we can prove that there exist such one n, that white can draw on a 4\*n board (with some kinds of rules), then "7-in-a-row is a draw" is proved.  In this article the authors tried n<=14 but failed.    
This branch is to solve the game mentioned in this article on 4\*n boards with large n.     
However katago shows that the min n is around 30, which means it is almost impossible to strictly solve the 4\*n game. So this method is almost dead.    
More details was written here: https://mathoverflow.net/questions/471302/can-gomokufive-in-a-row-draw-on-an-infinite-board-what-about-other-m-n-k-game     

## Other games
### DotsAndBoxes
It's a game that black and white alternately play edges. It is hard to represent edges in Katago, so I uses a (2H-1)\*(2W-1) board to represent the Dots-and-boxes board
Result: This is a very chaotic game, no strategies, just brute-force search. (probably because it is almost an "impartical game"). Katago can learn almost nothing in the first 1/3 game and there is only some strategies near the end of the game

    
    
    
    
    
    
    
    
    
    
    
-------------------------
-------------------------
-------------------------
# Old

***2023.1.8 Engine based on Katago 1.12 start Modifying***   

| Tasks                                                  | Branch         | Stage        | Notes                              |
| :----------------------------------------------------- | :------------- | :----------- | :--------------------------------- |
| 1 Compile and run training                             | Kata2023       | Finished     | -                                  |
| 2 Chinese Rule Only(Remove JP rules)                   | -     | Skipped    |   |
| 3 CaptureGo(Remove Go rules)                           | -    | Skipped         |   |
| 4 Black-White board games(Remove Capture)              | BW2023/BWnoscore2023 | Finished(outdated)         | Using Gomoku as a representative, very easy to be modified to other games. "BWnoscore2023" branch also removed komi and score   |
| 4.0 Remove score and komi                              | BWnoscore2023 | Finished(outdated)         |    |
| 4.1 Gomoku                                             | Gom2023        | Todo         | New Katagomo engine                |
| 5 Chess-like games(Multi-stage moves)                  | Movestone2023  | Finished(outdated)         | Use Breakthrough as a representative, very easy to be modified to other games    |
| 5.0 Only X-axis symmetry. And flip Y-axis when playing white. (For chess-like games) | MovestoneXsym2023  | Finished    | subtreeBias and patternBonus are also removed |
 




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

