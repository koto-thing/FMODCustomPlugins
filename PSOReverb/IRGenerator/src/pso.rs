use rand::Rng;

#[derive(Clone, Debug)]
pub struct Particle {
    pub position: Vec<f64>,      // 粒子の位置
    pub velocity: Vec<f64>,      // 粒子の速度
    pub best_position: Vec<f64>, // 粒子の最良位置
    pub best_error: f64,         // 粒子の最良位置での誤差
}

impl Particle {
    /// パーティクルの初期化関数
    /// - 引数
    ///     - `rng`: 乱数生成器
    /// - 戻り値
    ///     - Particle構造体
    pub fn new<R: Rng>(rng: &mut R) -> Self {
        let alpha = rng.random_range(0.5..10.0);    // 反射係数
        let feedback = rng.random_range(0.1..0.8);  // フィードバック量
        let damping = rng.random_range(0.1..0.9);   // ダンピング係数
        let mod_rate = rng.random_range(0.1..3.0);  // Hz
        let mod_depth = rng.random_range(0.0..5.0); // サンプル数

        Particle {
            position: vec![alpha, feedback, damping, mod_rate, mod_depth],
            velocity: vec![
                rng.random_range(-0.5..0.5),
                rng.random_range(-0.1..0.1),
                rng.random_range(-0.1..0.1),
                rng.random_range(-0.2..0.2),
                rng.random_range(-0.5..0.5)
            ],
            best_position: vec![alpha, feedback, damping, mod_rate, mod_depth],
            best_error: f64::MAX
        }
    }
}