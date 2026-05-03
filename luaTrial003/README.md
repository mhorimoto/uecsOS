# 試験用Luaコード

LuaコードによるM304Nの制御を試験するための代表的なコード

* BLD:0.3.12以降を推奨
* IPアドレスを指定して8888/udpでLuaコードをコマンドとして送り込む

## System関連

## lcd関連
LCD画面の表示機能に関するLuaのテスト  
lcd. を前置します。

### init()
初期化する関数。Lua側からはめったに呼ばれることはない。

### print()
文字列を表示する。  
```Lua
lcd.print("ABC TEST");
```
```sh
printf "lcd.print(\"uecs\")." | nc -u -w1  [IP Address] 8888
```
### clear()
全表示を消去する。
```Lua
lcd.clear();
```
```sh
printf "lcd.clear()." | nc -u -w1 [IP Address] 8888
```
### setCursor(x,y)
カーソルの位置を指定する。  
画面左上は(0,0)。

* x=0..19
* y=0..3

カーソル位置を移動してから、文字を表示する場合。

```Lua
lcd.setCursor(5,2);
lcd.print("IP Address");
```
```sh
printf "lcd.setCursor(5,2);\n lcd.print(\"IP Address\");" | nc -u -w1 [IP Address] 8888
```

## SD関連

## USB関連

## UECS関連

## 応用編

### シリアルにメッセージを出力してリセットする
```Lua
print("reset from Lua");
reset();
```
```sh
printf "print(\"reset from Lua\"); reset()." | nc -u -w1  [IP Address] 8888
```
### timeをLCDの左上に表示する
```Lua
lcd.clear();
a=uecs.time();
lcd.print(a);
```
```sh
printf "lcd.clear(); a=uecs.time(); lcd.print(a)." | nc -u -w1  [IP Address] 8888
```

