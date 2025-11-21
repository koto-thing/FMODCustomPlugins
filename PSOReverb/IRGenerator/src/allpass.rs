use std::f64::consts::PI;

#[derive(Clone)]
pub struct ModulatedAllPass {
    buffer: Vec<f64>,
    write_index: usize,
    pub feedback: f64,
    lfo_phase: f64,
    lfo_inc: f64,
    depth: f64,
}

impl ModulatedAllPass {
    /// オールパスフィルタの生成関数
    /// - 引数
    ///     - `delay_samples`: 遅延時間(サンプル数)
    ///     - `feedback`: フィードバック量
    ///     - `mod_rate`: LFOの変調周波数(Hz)
    ///     - `mod_depth`: LFOの変調深度(サンプル数)
    ///     - `sample_rate`: サンプリング周波数(Hz)
    /// - 戻り値
    ///     - ModulatedAllPass構造体
    pub fn new(delay_samples: usize, feedback: f64, mod_rate: f64, mod_depth: f64, sample_rate: f64) -> Self {
        Self {
            buffer: vec![0.0; delay_samples],
            write_index: 0,
            feedback,
            lfo_phase: 0.0,
            lfo_inc: 2.0 * PI * mod_rate / sample_rate,
            depth: mod_depth,
        }
    }

    /// オールパスフィルタの処理
    /// - 引数
    ///     - `input`: 入力サンプル
    /// - 戻り値
    ///     - 出力サンプル
    pub fn process(&mut self, input: f64) -> f64 {
        let buffer_len = self.buffer.len() as f64;

        // 変調量の計算
        self.lfo_phase += self.lfo_inc;
        if self.lfo_phase > 2.0 * PI {
            self.lfo_phase -= 2.0 * PI;
        }

        // 遅延位置の計算
        let modulation = self.lfo_phase.sin() * self.depth;
        let read_pos = self.write_index as f64 + buffer_len - buffer_len + modulation;

        // 読み込み位置のラップアラウンド処理
        let mut r = read_pos;

        // リングバッファのラップアラウンド処理
        while r < 0.0 { r += buffer_len; }
        while r >= buffer_len { r -= buffer_len; }

        //　線形補間による遅延信号の取得
        let index_base = r.floor() as usize;
        let index_next = (index_base + 1) % self.buffer.len();
        let frac = r - index_base as f64;

        // 線形補間する
        let delayed = self.buffer[index_base] * (1.0 - frac) + self.buffer[index_next] * frac;

        // オールパスフィルタの計算
        let output = -self.feedback * input + delayed;

        // リミッターをつけたフェードバック
        let mut to_buffer = input + self.feedback * output;
        if to_buffer > 2.0 { to_buffer = 2.0; }
        else if to_buffer < -2.0 { to_buffer = -2.0; }

        // バッファへの書き込み
        self.buffer[self.write_index] = to_buffer;

        // 書き込み位置の更新
        self.write_index += 1;
        if self.write_index >= self.buffer.len() {
            self.write_index = 0;
        }

        output
    }
}