# 📐 LaTeX 수식 실시간 미리보기

오일러 등식 $e^{i\pi} + 1 = 0$ 및 적분:
$$
\int_{-\infty}^{\infty}
  e^{-x^2}\,dx = \sqrt{\pi}
$$
테일러 전개 및 회전 행렬 $\det R(\theta)=1$:
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
  A["입력 $$x(t)$$"]
  B["변환 $$\hat{X}(\omega)$$"]
  C["출력 $$Y(\omega)$$"]
  A --> B --> C
```