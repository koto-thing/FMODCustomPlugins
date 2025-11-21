// 標準ライブラリと外部クレートのインポート
use eframe::egui;
use hound;
use rand::Rng;
use std::f64::consts::E;
use std::thread;
use std::sync::mpsc::{channel, Receiver, Sender};

// モジュールのインポート
mod biquad;
mod pso;
mod allpass;

// モジュールからの構造体のインポート
use biquad::Biquad;
use pso::Particle;
use allpass::ModulatedAllPass;

// 定数定義
const SAMPLE_RATE: f64 = 44100.0;  // サンプリング周波数
const NUM_PARTICLES: usize = 30;   // 粒子数
const MAX_ITERATIONS: usize = 100; // 最大反復回数

// フィルタ設定
const CROSSOVER_FREQ: f64 = 2000.0; // クロスオーバー周波数
const VELVET_DENSITY: f64 = 2000.0; // ベルベットノイズの密度

// PSOパラメータ
const W: f64 = 0.7;  // 慣性重み
const C1: f64 = 1.5; // 慣性係数
const C2: f64 = 1.5; // 認知係数と社会係数

// PSOパラメータのインデックス
const IDX_ALPHA: usize = 0;      // 減衰係数
const IDX_FEEDBACK: usize = 1;   // 拡散フィードバック係数
const IDX_DAMPING: usize = 2;    // ダンピング比
const IDX_MOD_RATE: usize = 3;   // モジュレーションレート
const IDX_MOD_DEPTH: usize = 4;  // モジュレーション深度
const NUM_DIMENSIONS: usize = 5; // 最適化するパラメータの数

/// 指定されたパラメータで減衰インパルス応答を生成する関数
/// - パラメータ:
///     - alpha_low: 低域の減衰率
///     - feedback: 拡散フィードバック係数
///     - damping_ratio: ダンピング比
///     - mod_rate: モジュレーションレート
///     - mod_depth: モジュレーション深度
///     - duration_sec: インパルス応答の長さ（秒）
/// - 戻り値:
///     - 生成されたインパルス応答のベクター
fn generate_damped_ir(alpha_low: f64, feedback: f64, damping_ratio: f64, mod_rate: f64, mod_depth: f64, duration_sec: f64) -> Vec<f64> {
    let num_samples = (duration_sec * SAMPLE_RATE) as usize;
    let mut rng = rand::rng();
    let mut ir = vec![0.0; num_samples];

    // 拡散用のオールパスフィルタの作成
    let mut apf1 = ModulatedAllPass::new(227, feedback, mod_rate, mod_depth, SAMPLE_RATE);
    let mut apf2 = ModulatedAllPass::new(337, feedback, mod_rate * 1.1, mod_depth * 0.9, SAMPLE_RATE);
    let mut apf3 = ModulatedAllPass::new(113, feedback * 0.8, mod_rate * 1.3, mod_depth * 0.8, SAMPLE_RATE);

    // フィルタの初期化
    let mut lpf = Biquad::new_lowpassfilter(CROSSOVER_FREQ, SAMPLE_RATE);
    let mut hpf = Biquad::new_highpassfilter(CROSSOVER_FREQ, SAMPLE_RATE);

    // 高域の減衰率を計算
    let alpha_high = if damping_ratio > 0.001 {
        alpha_low / damping_ratio
    } else {
        alpha_low * 1000.0
    };

    // 初期反射を作成
    let num_early_reflections = rng.random_range(5..15);
    for _ in 0..num_early_reflections {
        // 反射の遅延時間をランダムに決定
        let delay_time = rng.random_range(0.005..0.08);
        let index = (delay_time * SAMPLE_RATE) as usize;

        // インデックスがバッファ範囲内であれば反射を追加
        if index < num_samples {
            let amplitude = rng.random_range(0.5..1.0) * E.powf(-alpha_low * delay_time);
            let signal = if rng.random_bool(0.5) { 1.0 } else { -1.0 };
            ir[index] += amplitude * signal;
        }
    }

    // 後部残響を追加する
    let start_offset = (0.02 * SAMPLE_RATE) as usize;
    for i in start_offset..num_samples {
        let t = (i - start_offset) as f64 / SAMPLE_RATE;

        // ベルベットノイズの生成
        let probability = (VELVET_DENSITY) / SAMPLE_RATE;
        let noise = if rng.random_bool(probability) {
            if rng.random_bool(0.5) { 1.0 } else { -1.0 }
        } else {
            0.0
        };

        // フィルタ処理
        let low_signal = lpf.process(noise);
        let high_signal = hpf.process(low_signal);

        // 減衰エンベロープの適用
        let envelope_low = E.powf(-alpha_low * t);
        let envelope_high = E.powf(-alpha_high * t);

        // ミックス
        let mixed_sample = (high_signal * envelope_high) + (low_signal * envelope_low);

        // インパルス応答に追加
        ir[i] += mixed_sample;
    }

    // オールパスフィルタで拡散処理
    let mut diffused_ir = Vec::with_capacity(ir.len());
    for sample in ir.into_iter() {
        let s1 = apf1.process(sample);
        let s2 = apf2.process(s1);
        let s3 = apf3.process(s2);
        diffused_ir.push(s3);
    }

    diffused_ir
}

/// インパルス応答を評価する関数
/// - パラメータ:
///     - alpha: 減衰係数
///     - feedback: 拡散フィードバック係数
///     - damping: ダンピング比
///     - mod_rate: モジュレーションレート
///     - mod_depth: モジュレーション深度
///     - target_rt60: 目標RT60
///     - target_brightness: 目標明るさ
/// - 戻り値:
///     - 評価スコア（小さいほど良い）
fn evaluate_ir(alpha: f64, feedback: f64, damping: f64, mod_rate: f64, mod_depth: f64, target_rt60: f64, target_brightness: f64) -> f64 {
    if alpha <= 0.01 || feedback < 0.0 || feedback >= 0.98 || damping <= 0.05 || damping >= 1.0 {
        return 1000.0;
    }

    if mod_rate < 0.0 || mod_depth < 0.0 {
        return 1000.0;
    }

    // RT60の推定
    let decay_stretch = 1.0 - feedback.powi(2);
    let estimated_rt60 = (6.907755 / alpha) / decay_stretch;
    let rt60_error = (target_rt60 - estimated_rt60).abs();

    // 明るさ誤差を計算
    let brightness_error = (damping - target_brightness).abs();

    // トーンペナルティの計算
    let tone_penalty = (feedback - 0.6).abs() * 0.5;

    // モジュレーションのペナルティ
    let mut stability_penalty = 0.0;

    if mod_depth > 3.0 {
        stability_penalty += (mod_depth - 3.0) * 1.0;
    }

    if mod_rate > 2.0 {
        stability_penalty += (mod_rate - 2.0) * 0.5;
    }

    // 総合エラーの計算
    rt60_error + brightness_error + tone_penalty + stability_penalty
}

/// バッファを正規化する関数
/// - パラメータ:
///     - buffer: 正規化するサンプルバッファの可変参照
/// - 戻り値:
///     - なし
fn normalize(buffer: &mut [f64]) {
    // 最大ピーク値を取得
    let max_peak =
        buffer.iter()
            .fold(0.0f64, |max, &x| max.max(x.abs()));
    
    // 安全なゲインを計算して適用
    if max_peak > 0.0 {
        let safe_gain = (1.0 / max_peak) * 0.99;
        for sample in buffer.iter_mut() { *sample *= safe_gain; }
    }
}

/// クロスディフュージョンを適用する関数
/// - パラメータ:
///     - left: 左チャンネルのサンプルバッファの可変参照
///     - right: 右チャンネルのサンプルバッファの可変参照
/// - 戻り値:
///     - なし
fn apply_cross_diffusion(left: &mut Vec<f64>, right: &mut Vec<f64>) {
    let len = left.len();

    // 混ぜ合わせる強さ
    let cross_feed = 0.25;
    // 音が反対側に届くまでの遅延
    let delay_samples = (0.01 * SAMPLE_RATE) as usize;

    // オリジナルのコピーを作成
    let l_copy = left.clone();
    let r_copy = right.clone();

    // クロスディフュージョンの適用
    for i in delay_samples..len {
        left[i] = l_copy[i] + r_copy[i - delay_samples] * cross_feed;
        right[i] = r_copy[i] + l_copy[i - delay_samples] * cross_feed;
    }

    // 正規化の適用
    normalize(left);
    normalize(right);
}

/// eframeアプリケーションの構造体定義
pub struct IRGeneratorApp {
    target_rt60: f64,
    target_brightness: f64,
    status_message: String,
    is_generating: bool,
    progress_receiver: Option<Receiver<String>>,
}

/// eframeアプリケーションのデフォルト実装
impl Default for IRGeneratorApp {
    fn default() -> Self {
        Self {
            target_rt60: 3.0,
            target_brightness: 0.6,
            status_message: "Ready".to_string(),
            is_generating: false,
            progress_receiver: None,
        }
    }
}

/// eframeアプリケーションの実装
impl eframe::App for IRGeneratorApp {
    /// UIの更新関数
    /// - パラメータ:
    ///     - _ctx: eguiのコンテキスト
    ///     - _frame: eframeのフレーム
    /// - 戻り値:
    ///     - なし
    fn update(&mut self, _ctx: &egui::Context, _frame: &mut eframe::Frame) {
        let panel = egui::CentralPanel::default();

        // UIの構築
        panel.show(_ctx, |ui| {
            // タイトルと区切り線
            ui.heading("IR Generator with PSO Optimization");
            ui.separator();

            // パラメータ設定スライダー
            ui.add_enabled_ui(!self.is_generating, |ui| {
                ui.label("Target RT60 (sec): ");
                ui.add(egui::Slider::new(&mut self.target_rt60, 0.5..=10.0).text("seconds"));

                ui.label("Target Brightness: ");
                ui.add(egui::Slider::new(&mut self.target_brightness, 0.1..=0.9).text("0.1(Dark) - 0.9(Bright)"));
            });

            ui.add_space(10.0);

            // 生成ボタンと進捗表示
            if self.is_generating {
                ui.spinner();
                ui.label("Generating IR... Please wait.");
            } else {
                if ui.button("Generate Impulse Response").clicked() {
                    if let Some(path) = rfd::FileDialog::new()
                        .add_filter("WAV", &["wav"])
                        .set_file_name("impulse_response.wav")
                        .save_file() {
                        self.is_generating = true;
                        self.status_message = "Starting optimization...".to_string();

                        // 非同期でIR生成
                        let (tx, rx) = channel();
                        self.progress_receiver = Some(rx);

                        let rt60 = self.target_rt60;
                        let brightness = self.target_brightness;
                        let path_buffer = path;

                        thread::spawn(move || {
                            run_pso_and_save(rt60, brightness, path_buffer, tx);
                        });
                    }
                }
            }

            // ステータスメッセージの表示
            ui.separator();
            ui.label(format!("Status: {}", self.status_message));

            // スレッドからの進捗更新を受け取る
            if let Some(rx) = &self.progress_receiver {
                while let Ok(msg) = rx.try_recv() {
                    if msg == "DONE" {
                        self.is_generating = false;
                        self.status_message = "Saved Successfully!".to_string();
                        self.progress_receiver = None;
                        break;
                    } else {
                        self.status_message = msg;
                    }
                }
            }
        });

        // 生成中は再描画を要求
        if self.is_generating {
            _ctx.request_repaint();
        }
    }
}

/// PSOを実行してIRを生成・保存する関数
/// - パラメータ:
///     - target_rt60: 目標RT60
///     - target_brightness: 目標明るさ
///     - path: 保存先のファイルパス
///     - tx: 進捗メッセージを送信するチャネルの送信側
/// - 戻り値:
///     - なし
fn run_pso_and_save(target_rt60: f64, target_brightness: f64, path: std::path::PathBuf, tx: Sender<String>) {
    // 粒子の初期化
    let mut rng = rand::rng();
    let mut particles: Vec<Particle> = (0..NUM_PARTICLES)
        .map(|_| Particle::new(&mut rng))
        .collect();

    // グローバルベストの初期化
    let mut global_best_position = vec![
        particles[0].position[0],
        particles[0].position[1],
        particles[0].position[2],
        particles[0].position[3],
        particles[0].position[4]
    ];
    let mut global_best_error = f64::MAX;

    // 最適化ループ
    for iter in 0..MAX_ITERATIONS {
        // 各粒子の評価
        for p in &mut particles {
            // パラメータの取得
            let alpha = p.position[IDX_ALPHA];
            let feedback = p.position[IDX_FEEDBACK];
            let damping = p.position[IDX_DAMPING];
            let mod_rate = p.position[IDX_MOD_RATE];
            let mod_depth = p.position[IDX_MOD_DEPTH];

            // IRを評価
            let error = evaluate_ir(alpha, feedback, damping, mod_rate, mod_depth, target_rt60, target_brightness);

            // 個体ベストの更新
            if error < p.best_error {
                p.best_error = error;
                p.best_position = vec![
                    p.position[0], p.position[1], p.position[2], p.position[3], p.position[4]
                ];
            }

            // グローバルベストの更新
            if error < global_best_error {
                global_best_error = error;
                global_best_position = vec![
                    p.position[0], p.position[1], p.position[2], p.position[3], p.position[4]
                ];
            }
        }

        // 粒子の位置と速度の更新
        for p in &mut particles {
            let r1: f64 = rng.random();
            let r2: f64 = rng.random();

            for d in 0..NUM_DIMENSIONS {
                let cog = C1 * r1 * (p.best_position[d] - p.position[d]);
                let soc = C2 * r2 * (global_best_position[d] - p.position[d]);

                p.velocity[d] = W * p.velocity[d] + cog + soc;
                p.position[d] += p.velocity[d];
            }

            // パラメータの制限
            // Alpha
            if p.position[IDX_ALPHA] < 0.1 { p.position[IDX_ALPHA] = 0.1; }
            // Feedback
            if p.position[IDX_FEEDBACK] < 0.0 { p.position[IDX_FEEDBACK] = 0.0; }
            if p.position[IDX_FEEDBACK] > 0.9 { p.position[IDX_FEEDBACK] = 0.9; }
            // Damping (0.1 ~ 0.99)
            if p.position[IDX_DAMPING] < 0.1 { p.position[IDX_DAMPING] = 0.1; }
            if p.position[IDX_DAMPING] > 0.99 { p.position[IDX_DAMPING] = 0.99; }
            // Mod Rate (0.1 ~ 5.0 Hz)
            if p.position[IDX_MOD_RATE] < 0.1 { p.position[IDX_MOD_RATE] = 0.1; }
            if p.position[IDX_MOD_RATE] > 5.0 { p.position[IDX_MOD_RATE] = 5.0; }
            // Mod Depth (0.0 ~ 10.0 samples)
            if p.position[IDX_MOD_DEPTH] < 0.0 { p.position[IDX_MOD_DEPTH] = 0.0; }
            if p.position[IDX_MOD_DEPTH] > 10.0 { p.position[IDX_MOD_DEPTH] = 10.0; }
        }

        // 進捗の表示
        if iter % 10 == 0 {
            println!("Iter {}: Err={:.4}", iter, global_best_error);
        }
    }

    // 最良パラメータでIRを生成して保存
    let _ = tx.send("Generating Impulse Response...".to_owned());

    // 最良パラメータの取得
    let best_alpha = global_best_position[IDX_ALPHA];
    let best_feedback = global_best_position[IDX_FEEDBACK];
    let best_damping = global_best_position[IDX_DAMPING];
    let best_mod_rate = global_best_position[IDX_MOD_RATE];
    let best_mod_depth = global_best_position[IDX_MOD_DEPTH];

    // IRの生成
    let duration = target_rt60 * 1.2;
    let mut ir_l = generate_damped_ir(best_alpha, best_feedback, best_damping, best_mod_rate, best_mod_depth, duration);
    let mut ir_r = generate_damped_ir(best_alpha, best_feedback, best_damping, best_mod_rate, best_mod_depth, duration);

    // クロスディフュージョンの適用
    apply_cross_diffusion(&mut ir_l, &mut ir_r);

    // Wavファイルの書き出し設定
    let spec = hound::WavSpec {
        channels: 2,
        sample_rate: SAMPLE_RATE as u32,
        bits_per_sample: 32,
        sample_format: hound::SampleFormat::Float,
    };

    // Wavファイルの書き出し
    if let Ok(mut writer) = hound::WavWriter::create(path, spec) {
        for (l, r) in ir_l.into_iter().zip(ir_r.into_iter()) {
            writer.write_sample(l as f32).unwrap();
            writer.write_sample(r as f32).unwrap();
        }
        writer.finalize().unwrap();
        let _ = tx.send("DONE".to_owned());
    } else {
        let _ = tx.send("Error: Could not save file".to_owned());
    }
}

/// メイン関数
/// - 戻り値:
///     - eframe::Result<()>
fn main() -> eframe::Result<()> {
    // ウィンドウオプションの設定
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([400.0, 300.0])
            .with_min_inner_size([300.0, 200.0]),
        ..Default::default()
    };

    // eframeアプリケーションの起動
    eframe::run_native(
        "IR Generator with PSO Optimization",
        options,
        Box::new(|_cc| {
            Ok(Box::new(IRGeneratorApp::default()))
        }),
    )
}