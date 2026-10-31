# 📐 LaTeX Math & Scientific Preview

Euler's identity $e^{i\pi} + 1 = 0$:
$$
\int_{-\infty}^{\infty}
  e^{-x^2}\,dx = \sqrt{\pi}
$$
Taylor Series & Rotation $\det R(\theta)=1$:
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
  A["Input $$x(t)$$"]
  B["FFT $$\hat{X}(\omega)$$"]
  C["Out $$Y(\omega)$$"]
  A --> B --> C
```