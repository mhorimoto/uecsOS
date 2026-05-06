-- 単行コメントは2つのハイフンで始まる

--[[
     複数行コメント
--]]

----------------------------------------------------
-- 1. 変数と制御フロー
----------------------------------------------------

num = 42  -- すべての数値は倍精度浮動小数点型。
-- 心配無用、64ビットの倍精度浮動小数点数のうち52ビットは
-- 正確な整数値を保存するために使われる; 52ビット以内の整数値なら
-- 精度の問題を心配する必要はない。

s = 'walternate'  -- Pythonと同様、文字列は不変。
t = "ダブルクォートも使える"
u = [[ 複数行の文字列は
       2つの角括弧で
       開始と終了を表す。]]
t = nil  -- tの定義を取り消す; Lua はガベージコレクションをサポート。

-- ブロックはdo/endなどのキーワードで識別される：
while num < 50 do
  num = num + 1  -- ++ や += 演算子はサポートされていない。
end

-- If文：
if num > 40 then
  print('over 40')
elseif s ~= 'walternate' then  -- ~= は「等しくない」を表す。
  -- Pythonと同様、== で等しいかチェック；文字列も同様。
  io.write('not over 40\n')  -- デフォルトは標準出力。
else
  -- デフォルトはグローバル変数。
  thisIsGlobal = 5  -- 通常はキャメルケースを使う。

  -- ローカル変数の定義方法：
  local line = io.read()  -- 標準入力の次の行を読む。

  -- .. 演算子は文字列の連結に使う：
  print('Winter is coming, ' .. line)
end

-- 未定義の変数はnilを返す。
-- これはエラーではない：
foo = anUnknownVariable  -- 現在 foo = nil.

aBoolValue = false

-- nilとfalseのみが偽; 0と ''は真！
if not aBoolValue then print('false') end

-- 'or'と'and'は短絡評価される
-- C/jsの a?b:c 演算子に似ている：
ans = aBoolValue and 'yes' or 'no'  --> 'no'

karlSum = 0
for i = 1, 100 do  -- 範囲は両端を含む
  karlSum = karlSum + i
end

-- "100, 1, -1" で減少する範囲を表す：
fredSum = 0
for j = 100, 1, -1 do fredSum = fredSum + j end

-- 一般に、範囲表現は begin, end[, step] の形式。

-- ループのもう1つの構造：
repeat
  print('the way of the future')
  num = num - 1
until num == 0

----------------------------------------------------
-- 2. 関数
----------------------------------------------------

function fib(n)
  if n < 2 then return n end
  return fib(n - 2) + fib(n - 1)
end

-- クロージャと無名関数をサポート：
function adder(x)
  -- adderを呼び出すと、返される関数が作成され、
  -- xの値が記憶される：
  return function (y) return x + y end
end
a1 = adder(9)
a2 = adder(36)
print(a1(16))  --> 25
print(a2(64))  --> 100

-- 戻り値、関数呼び出し、代入は
-- 長さが一致しないリストを使用できる。
-- 一致しない受け取り側にはnilが代入される；
-- 一致しない送り側は破棄される。

x, y, z = 1, 2, 3, 4
-- x = 1、y = 2、z = 3、そして 4 は破棄される。

function bar(a, b, c)
  print(a, b, c)
  return 4, 8, 15, 16, 23, 42
end

x, y = bar('zaphod')  --> "zaphod  nil nil" を出力
-- 現在 x = 4, y = 8、そして値15..42は破棄される。

-- 関数はファーストクラスオブジェクトで、ローカルにもグローバルにもできる。
-- 以下の表現は等価：
function f(x) return x * x end
f = function (x) return x * x end

-- これらも等価：
local function g(x) return math.sin(x) end
local g; g = function (x) return math.sin(x) end
-- 上記は'local g'により、gが自己参照できる。
local g = function(x) return math.sin(x) end
-- これは local function g(x)... と等価だが、関数本体でgは自己参照できない

-- ちなみに、三角関数はラジアン単位。

-- 文字列引数1つで関数を呼び出す場合、括弧を省略できる：
print 'hello'  -- 動作する。

-- 関数呼び出しでtable引数が1つだけの場合も、
-- 括弧を省略できる（tableの詳細は後述）：
print {} -- これも動作する。

----------------------------------------------------
-- 3. Table
----------------------------------------------------

-- Table = Lua唯一の複合データ構造;
--         連想配列である。
-- PHPの配列やJavaScriptのオブジェクトに似ていて、
-- ハッシュテーブルや辞書であり、リストとしても使える。

-- 辞書/mapとしてTableを使う：

-- Dict リテラルはデフォルトで文字列型のキーを使う：
t = {key1 = 'value1', key2 = false}

-- 文字列キーはJavaScriptのようなドット記法が使える：
print(t.key1)  -- 'value1' を出力。
t.newKey = {}  -- 新しいキーと値のペアを追加。
t.key2 = nil   -- tableからkey2を削除。

-- nil以外の任意の値をキーとして使用：
u = {['@!#'] = 'qbert', [{}] = 1729, [6.28] = 'tau'}
print(u[6.28])  -- "tau" を出力

-- 数値と文字列のキーは値で一致するが、
-- tableは識別子で一致する。
a = u['@!#']  -- 現在 a = 'qbert'.
b = u[{}]     -- 1729を期待するかもしれないが、nilが得られる:
-- b = nil、なぜなら見つからないから。
-- 見つからないのは、使用したキーがデータ保存時に使ったものと
-- 同じオブジェクトではないため。
-- したがって文字列と数値の方が移植性の高いキー。

-- table引数1つだけの関数呼び出しには括弧が不要：
function h(x) print(x.key1) end
h{key1 = 'Sonmi~451'}  -- 'Sonmi~451'を出力。

for key, val in pairs(u) do  -- Tableを走査
  print(key, val)
end

-- _G は特殊なtableで、すべてのグローバル変数を保持する
print(_G['_G'] == _G)  -- 'true'を出力。

-- リスト/配列としての使用：

-- リストリテラルは暗黙的に整数キーを追加：
v = {'value1', 'value2', 1.21, 'gigawatts'}
for i = 1, #v do  -- #v はリストのサイズ
  print(v[i])  -- インデックスは 1 から始まる!! クレイジーだ！
end
-- 'list'は本当の型ではなく、v は実際にはtableで、
-- 連続した整数をキーとして使用しているだけで、リストのように使える。

----------------------------------------------------
-- 3.1 メタテーブル（metatable）とメタメソッド（metamethod）
----------------------------------------------------

-- tableのメタテーブルは、演算子オーバーロードのような
-- 動作をサポートする仕組みを提供する。
-- 後でメタテーブルがJavaScriptのprototypeのような動作を
-- どのようにサポートするか見ていく。

f1 = {a = 1, b = 2}  -- 分数 a/b を表す。
f2 = {a = 2, b = 3}

-- これは失敗する：
-- s = f1 + f2

metafraction = {}
function metafraction.__add(f1, f2)
  local sum = {}
  sum.b = f1.b * f2.b
  sum.a = f1.a * f2.b + f2.a * f1.b
  return sum
end

setmetatable(f1, metafraction)
setmetatable(f2, metafraction)

s = f1 + f2  -- f1のメタテーブルの__add(f1, f2)メソッドを呼び出す

-- f1, f2 はメタテーブルに関するキーを持たない、これはJavaScriptの
-- prototypeとは異なる。
-- したがってgetmetatable(f1)でメタテーブルを取得する必要がある。
-- メタテーブルは通常のtableで、
-- メタテーブルのキーは通常のLuaのキー、例えば__add。

-- しかし次の行は失敗する、なぜならsにメタテーブルがないから：
-- t = s + s
-- 以下で提供されるクラスに似たパターンでこれを解決できる：

-- メタテーブルの__index は検索のドット演算子をオーバーロードできる：
defaultFavs = {animal = 'gru', food = 'donuts'}
myFavs = {food = 'pizza'}
setmetatable(myFavs, {__index = defaultFavs})
eatenBy = myFavs.animal  -- 動作する！メタテーブルのおかげ

-- tableで直接キーの検索に失敗した場合、
-- メタテーブルの__index を使って再帰的に再試行する。

-- __indexの値はfunction(tbl, key)でもよい
-- これによりカスタム検索をサポートできる。

-- __index、__addなどの値は、メタメソッドと呼ばれる。
-- 以下はtableメタメソッドの一覧：

-- __add(a, b)                     a + b 用
-- __sub(a, b)                     a - b 用
-- __mul(a, b)                     a * b 用
-- __div(a, b)                     a / b 用
-- __mod(a, b)                     a % b 用
-- __pow(a, b)                     a ^ b 用
-- __unm(a)                        -a 用
-- __concat(a, b)                  a .. b 用
-- __len(a)                        #a 用
-- __eq(a, b)                      a == b 用
-- __lt(a, b)                      a < b 用
-- __le(a, b)                      a <= b 用
-- __index(a, b)  <関数またはtable>  a.b 用
-- __newindex(a, b, c)             a.b = c 用
-- __call(a, ...)                  a(...) 用

----------------------------------------------------
-- 3.2 クラスに似たtableと継承
----------------------------------------------------

-- Luaにはクラスが組み込まれていない；様々な方法で、tableと
-- メタテーブルを使ってクラスを実装できる。

-- 以下は例で、解説は後述：

Dog = {}                                   -- 1.

function Dog:new()                         -- 2.
  local newObj = {sound = 'woof'}          -- 3.
  self.__index = self                      -- 4.
  return setmetatable(newObj, self)        -- 5.
end

function Dog:makeSound()                   -- 6.
  print('I say ' .. self.sound)
end

mrDog = Dog:new()                          -- 7.
mrDog:makeSound()  -- 'I say woof'         -- 8.

-- 1. Dogはクラスのように見える；実際にはtableである。
-- 2. 関数 tablename:fn(...) は
--    関数 tablename.fn(self, ...) と等価
--    コロン（:）は単にselfを第一引数として追加する。
--    7 & 8 を読んでself変数がどのように値を得るか理解しよう。
-- 3. newObjはクラスDogのインスタンス。
-- 4. self = 継承されるクラス。通常self = Dogだが、継承で変わることがある。
--    newObjのメタテーブルと__indexの両方をselfに設定すると、
--    newObjはselfの関数を取得できる。
-- 5. 覚えておこう：setmetatableは第一引数を返す。
-- 6. コロン（:）の働きは2と同じだが、ここでは
--    selfはクラスではなくインスタンス
-- 7. Dog.new(Dog)と等価、したがってnew()内で、self = Dog。
-- 8. mrDog.makeSound(mrDog)と等価; self = mrDog。

----------------------------------------------------

-- 継承の例：

LoudDog = Dog:new()                           -- 1.

function LoudDog:makeSound()
  local s = self.sound .. ' '                 -- 2.
  print(s .. s .. s)
end

seymour = LoudDog:new()                       -- 3.
seymour:makeSound()  -- 'woof woof woof'      -- 4.

-- 1. LoudDogはDogのメソッドと変数のリストを取得する。
-- 2. new()のおかげで、selfは'sound'キーを持つ、3を参照。
-- 3. LoudDog.new(LoudDog)と等価、変換すると
--    Dog.new(LoudDog)となる、なぜならLoudDogには'new'キーがなく、
--    そのメタテーブルに __index = Dog があるから。
--    結果: seymourのメタテーブルはLoudDogで、
--    LoudDog.__index = Dog。したがってseymour.key
--    = seymour.key, LoudDog.key, Dog.key
--    のうち最初に指定されたキーを持つtableから取得。
-- 4. 'makeSound'キーはLoudDogで見つかる；
--    LoudDog.makeSound(seymour)と等価。

-- 必要であれば、サブクラスも基底クラスと同様にnew()を持てる：
function LoudDog:new()
  local newObj = {}
  -- newObjを初期化
  self.__index = self
  return setmetatable(newObj, self)
end

----------------------------------------------------
-- 4. モジュール
----------------------------------------------------


--[[ この部分をコメントアウトしたので、スクリプトの残りの部分が実行できる
-- ファイルmod.luaの内容がこのようだと仮定：
local M = {}

local function sayMyName()
  print('Hrunkner')
end

function M.sayHello()
  print('Why hello there')
  sayMyName()
end

return M

-- 別のファイルでmod.luaの機能を使用できる：
local mod = require('mod')  -- ファイルmod.luaを実行。
-- 注意：require は LUA_PATH と一緒に使う必要がある 例：export LUA_PATH="$HOME/workspace/projectName/?.lua;;"

-- requireはモジュールを含める標準的な方法。
-- requireは次と等価:     (キャッシュされていない場合；後述の内容を参照)
local mod = (function ()
  <mod.luaの内容>
end)()
-- mod.luaは関数本体にラップされているため、mod.luaのローカル変数は
-- 外部から見えない。

-- 以下のコードは動作する、なぜならここでmod = mod.lua の M だから：
mod.sayHello()  -- Hrunknerに挨拶する。

-- これは誤り；sayMyNameはmod.lua内にのみ存在する：
mod.sayMyName()  -- エラー

-- requireが返す値はキャッシュされるので、ファイルは一度だけ実行される、
-- 複数回requireされても。

-- mod2.luaがコード"print('Hi!')"を含むと仮定。
local a = require('mod2')  -- Hi!を出力
local b = require('mod2')  -- もう出力しない; a=b.

-- dofileはrequireに似ているが、キャッシュしない：
dofile('mod2')  --> Hi!
dofile('mod2')  --> Hi! (再度実行、requireと異なる)

-- loadfileはluaファイルを読み込むが、実行しない。
f = loadfile('mod2')  -- f()を呼ぶとmod2.luaが実行される。

-- loadstringはloadfileの文字列版。
-- (loadstringは非推奨、loadを使用)
g = load('print(343)')  -- 関数を返す。
g()  -- 343を出力; それ以前は何も出力しない。

--]]

