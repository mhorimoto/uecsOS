# LuaTrial001
8888/udpで投入されたLuaプログラムを実行する．

## 投入方法

### LED ON/OFFのテスト
```bash
echo -e digitalWrite(13, 0)\nprint('LED OFF')\n." | nc -u -w1 192.168.38.106 8888
echo -e "." | nc -u -w1 192.168.38.106 8888
echo -e "digitalWrite(13, 1)\nprint('LED ON')" | nc -u -w1 192.168.38.106 8888
echo -e "." | nc -u -w1 192.168.38.106 8888
```

### SDメモリのテスト
```bash
echo -e "local f=dir('/')\nfor i,v in ipairs(f) do print(i..': '..v) end" | nc -u -w1 192.168.38.106 8888
echo -e "." | nc -u -w1 192.168.38.106 8888
```
