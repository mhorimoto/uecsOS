# trial001
8888/udpで投入されたBASICプログラムを実行する．

## 投入方法
### PRINT文のテスト
```bash
echo -e "print \"Test Message\n\"" | nc -u -w1 192.168.38.106 8888
```
シリアルモニターに
```console
Received new logic via UDP.
--- Executing New Logic ---
Test Message
--- Execution Finished ---
```
と表示されます．
