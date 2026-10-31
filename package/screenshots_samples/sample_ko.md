# ⚡ miu v1.0.38 — Markdown 실시간 미리보기

<kbd>Alt</kbd> + <kbd>P</kbd>를 눌러 **CommonMark & GFM** 분할 미리보기를 즉시 실행하세요 (`150ms` 디바운스 / 외부 브라우저 불필요).

> [!TIP]
> **1GB 이상의 대용량 파일도 즉시 미리보기**
> 디스크 복사 없이 첫 `512KB`를 UTF-8 경계에 맞춰 안전하고 빠르게 렌더링합니다.

| 기능 항목 | 구문 예시 | 렌더링 엔진 | 상태 |
| :--- | :---: | :--- | ---: |
| **GFM 표** | `\| A \| B \|` / `\| :--- \|` | Native C++ + WebView2 | ✅ 완료 |
| **코드 & 밑줄 보호** | `` `a | b` `` / `snake_case` | 경계 이스케이프 분석 | ⚡ 0.8ms |
| **GFM 알림 블록** | `> [!NOTE]` / `> [!TIP]` | 다크/라이트 자동 연동 | 🎨 지원 |

- [x] **작업 목록 및 다단 중첩 목록**
  - 들여쓰기에 따른 `<ul>` / `<ol>` 자동 계층화 및 <kbd>F5</kbd> 새로고침
- [ ] 다음 업데이트: 사용자 지정 CSS 테마 확장

```cpp
// Native C++20 Direct2D + WebView2 Split Preview
void Editor::togglePreview() {
    isPreviewOpen = !isPreviewOpen;
    updatePreviewContent(); // Zero temp-file overhead
}
```
