# Chinese UI + cheat mode smoke: Chinese names and pinyin search (see zh.prelaunch.ps1).
$u = "zh"
Wait-LogLine "Recipe index rebuilt: [1-9]" 60 | Out-Null
Open-Inventory
Click (1155/1280) (692/720) -Relative
Type-Text "mub"
Press-Key ENTER
Move-Mouse (1070/1280) (70/720) -Relative
Start-Sleep -Milliseconds 700
Save-Screenshot "pinyin-search" $u | Out-Null
