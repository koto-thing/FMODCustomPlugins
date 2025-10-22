declare filename "BitCrasher.dsp";
declare name "BitCrasher";
import("stdfaust.lib");

// ビット深度とダウンサンプリングの度合いを整数スライダーで調整できるようにする
bits = hslider("bits", 8, 1, 16, 1) : int;
downsampling = hslider("downsampling", 4, 1, 32, 1) : int;

// ビット深度を擬似的に下げる関数
// 信号を一度整数に丸め、もう一度浮動小数点数に戻す
bitcrush(x) = int(x * (2^bits - 1)) / (2^bits - 1.0);

// カウンターを作成し、カウンターが0になるときにトリガーを生成する
// ba.sAndH (Sample and Hold) を使用して、トリガーが来た時の入力信号をホールドする(ダウンサンプリング, レートリダクション)
downsampler = ba.sAndH((+(1) ~ _) % downsampling == 0);

process = _ : downsampler : bitcrush;