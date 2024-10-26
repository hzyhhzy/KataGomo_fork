# Kata Ultimate Tic-tac-toe 2024
Latest release: https://github.com/hzyhhzy/KataGo/releases/tag/UltimateTictactoe_20241019   
Brief introduction of the rule: https://en.wikipedia.org/wiki/Ultimate_tic-tac-toe   
   
This AI includes a widely-accepted additional rule: If one should play in a finished (win, lose, or filled) sub-board, then he/she can play everywhere. Training katago without this additional rule is meaningless because it has been proved as first player win without this rule.   
   
This AI supports both with and without “Tiebreaker” rule.   
Tiebreaker: If no one connects 3 subboards, then who own more subboards wins.   


## Results
This game is complex enough to avoid being "weakly solved" by KataGo.   
With Tiebreaker: First player 70%, Second player 14%, Draw 16%   
Without Tiebreaker: First player 33%, Second player 11%, Draw 56%   


## Training schedule
Scripts and parameters used in this run are in **./scripts**   
   
Total training cost: **~8 RTX4090\*day**       
    
One model for two rules (with and without "Tiebreaker"). Each rule has 50% selfplay games.    
Model size is b10c384nbt, ~7M selfplay games in total.   