# ISSUE-002: SVG glow filters render as squares on Windows WebView2

## Reported
2026-01-22

## Description
On Windows, the SVG `feGaussianBlur` filters for glow effects render incorrectly as square/diamond shapes instead of smooth blurs. This affects:
- Compression curve glow
- Threshold point glow
- Knob arc glows

The "FAT" text glow works correctly because it uses CSS `text-shadow` instead of SVG filters.

On macOS (WebKit), everything renders correctly.

## Reproduction
1. Build plugin for Windows
2. Load in DAW or standalone
3. Observe compression graph and knobs
4. Glow appears as solid diamond/square instead of soft blur

## Investigation

### Files Examined
- `ui/public/index.html` - SVG filter definitions
- `ui/public/css/style.css` - CSS glow styles
- `ui/public/js/main.js` - JavaScript initialization

### Root Cause
WebView2 (Chromium/Edge on Windows) has known issues with SVG filters using `feGaussianBlur` combined with `filterUnits="objectBoundingBox"`. The blur calculation fails and renders a solid bounding box instead.

**Working (CSS text-shadow):**
```css
.fat-label {
  text-shadow: 0 0 10px #ff9500, 0 0 20px #ff9500;
}
```

**Broken (SVG feGaussianBlur):**
```xml
<filter id="curveGlow">
  <feGaussianBlur stdDeviation="4"/>
  ...
</filter>
<path filter="url(#curveGlow)" .../>
```

## Proposed Fix

### Changes
1. `ui/public/js/main.js`
   - Detect Windows platform via User-Agent
   - Add `windows` class to body element

2. `ui/public/css/style.css`
   - Add `.windows` class variants that use CSS `filter: drop-shadow()` instead of SVG filters
   - Keep SVG filters for macOS (works perfectly)

3. `ui/public/index.html`
   - Remove `filter="url(...)"` attributes from SVG elements (applied via CSS instead)

### Risk
Low - isolated to UI styling, no DSP changes

## Test Plan
1. Build on Windows
2. Verify glow effects match macOS appearance
3. Test all knobs and graph elements
4. Test in standalone and VST3 host

## Tasks
- [ISSUE] Fix: Add Windows platform detection and CSS fallback glows
- [ISSUE] Test: Verify glow rendering on Windows

## Resolved
[pending]

## Version
0.2.0 → 0.2.1
