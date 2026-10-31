# 📐 LaTeX 数式ライブプレビュー

オイラーの等式 $e^{i\pi} + 1 = 0$ と積分:
$$
\int_{-\infty}^{\infty}
  e^{-x^2}\,dx = \sqrt{\pi}
$$
テイラー展開と回転行列 $\det R(\theta)=1$:
$$
f(x) = \sum_{n=0}^{\infty}
  \frac{f^{(n)}(a)}{n!}(x-a)^n
$$
$$
R(\theta) = \begin{pmatrix}
  \cos\theta & -\sin\theta \\
  \sin\theta & \cos\theta
\end{pmatrix}
$$
```mermaid
graph LR
  A["入力 $$x(t)$$"]
  B["変換 $$\hat{X}(\omega)$$"]
  C["出力 $$Y(\omega)$$"]
  A --> B --> C
```