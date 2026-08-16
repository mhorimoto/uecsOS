# Lua Version
Luaを中間言語として用いるトライアル．

* print()
* digitalWrite()
* delay()

をインタプリタとして実装した．

## テストプログラム

以下のとおりで，内蔵LEDが5回点滅する．

```Lua
    print('LED Blink Start from Lua...') 
    for i=1, 5 do
      digitalWrite(13, 1)
      delay(200)
      digitalWrite(13, 0)
      delay(200)
      print('Blink count: ' .. i)
    end
    print('LED Blink Finished!')
```

## 留意点
lua言語プロセッサの各種調整を行ったものが本ディレクトリのluaサブディレクトリ配下にあるので，このluaサブディレクトリ毎コピーして使用する．

また，platformio.iniの内容も真似するとよい．
