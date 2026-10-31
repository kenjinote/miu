# 📐 LaTeX 数学公式实时预览

欧拉恒等式 $e^{i\pi} + 1 = 0$ 与高斯积分:
$$
\int_{-\infty}^{\infty}
  e^{-x^2}\,dx = \sqrt{\pi}
$$
泰勒级数展开与旋转矩阵 $\det R(\theta)=1$:
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
  A["输入 $$x(t)$$"]
  B["变换 $$\hat{X}(\omega)$$"]
  C["输出 $$Y(\omega)$$"]
  A --> B --> C
```