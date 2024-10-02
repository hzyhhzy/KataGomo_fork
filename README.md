# CaptureGo (2024.8)
This branch is for Capture-Go   
## Rule
Who firstly capture N stones wins. 1<=N<=5   
Pass is not allowed. If no one has enough captures, just continue, then one player will fill the eyes and die. In this case, it is like area-ish scoring with group tax(还棋头).    
   
So we can define komi(贴目) as the following:   
komi=0.5: Original rule, no one can pass   
komi=0.5+k: White can pass k times   
komi=0.5-k: Black can pass k times   
komi=k: White can pass k times, but if white has passed exactly k times until having enough captures, then the game is a draw   
komi=1-k: Black can pass k times, but if black has passed exactly k times until having enough captures, then the game is a draw   


