// FatPressor - Main UI Script
// Uses official JUCE WebView bindings for parameter control

import { getSliderState, getNativeFunction } from './juce/index.js';

// ============================================
// PARAMETER CONFIGURATION
// ============================================

const PARAMS = {
  threshold: { min: -60, max: 0, default: -20, unit: 'dB', decimals: 1 },
  ratio: { min: 1, max: 20, default: 4, unit: ':1', decimals: 1 },
  attack: { min: 0.1, max: 100, default: 10, unit: 'ms', decimals: 1 },
  release: { min: 10, max: 1000, default: 100, unit: 'ms', decimals: 0 },
  fat: { min: 0, max: 100, default: 50, unit: '%', decimals: 0 },
  output: { min: -12, max: 12, default: 0, unit: 'dB', decimals: 1 },
  mix: { min: 0, max: 100, default: 100, unit: '%', decimals: 0 }
};

// SVG Arc constants
// Circle radius = 46, circumference = 2 * PI * 46 = 289.03
// 270 degrees = 75% of circumference = 216.77
const ARC_CIRCUMFERENCE = 289.03;
const ARC_LENGTH = 216.77; // 270 degrees

// Knob rotation range: -135deg to +135deg (270 degrees total)
const KNOB_MIN_ANGLE = -135;
const KNOB_MAX_ANGLE = 135;

// State storage for slider states
const sliderStates = {};

// State for FAT label glow
let currentFatValue = 0;
let currentGainReduction = 0;
let currentOutputLevel = -60; // dB

// ============================================
// KNOB CONTROL
// ============================================

class KnobControl {
  constructor(knobElement, paramId, config) {
    this.knob = knobElement;
    this.paramId = paramId;
    this.config = config;
    this.isDragging = false;
    this.startY = 0;
    this.startValue = 0;
    this.sliderState = null;
    this.currentNormalized = 0;

    // Get UI elements
    this.arcEl = document.getElementById(`${paramId}Arc`);
    this.indicatorEl = document.getElementById(`${paramId}Indicator`);
    this.valueEl = document.getElementById(`${paramId}Value`);

    this.setupEvents();
    this.connectToJUCE();
  }

  connectToJUCE() {
    // Try to get JUCE slider state
    try {
      this.sliderState = getSliderState(this.paramId);
      sliderStates[this.paramId] = this.sliderState;

      // Listen for value changes from JUCE
      this.sliderState.valueChangedEvent.addListener(() => {
        const normalized = this.sliderState.getNormalisedValue();
        const scaled = this.sliderState.getScaledValue();
        console.log(`[FatPressor] valueChangedEvent for ${this.paramId}: normalized=${normalized}, scaled=${scaled}`);
        this.updateUI(normalized, scaled);
      });

      // Get initial value
      const normalized = this.sliderState.getNormalisedValue();
      const scaled = this.sliderState.getScaledValue();
      this.updateUI(normalized, scaled);

      console.log(`[FatPressor] Connected param: ${this.paramId}`);
    } catch (e) {
      console.warn(`[FatPressor] Could not connect ${this.paramId}:`, e);
      // Use default value for standalone testing
      const normalized = (this.config.default - this.config.min) / (this.config.max - this.config.min);
      this.updateUI(normalized, this.config.default);
    }
  }

  setupEvents() {
    this.knob.addEventListener('mousedown', this.onMouseDown.bind(this));
    document.addEventListener('mousemove', this.onMouseMove.bind(this));
    document.addEventListener('mouseup', this.onMouseUp.bind(this));

    // Double-click to reset
    this.knob.addEventListener('dblclick', this.onDoubleClick.bind(this));
  }

  onMouseDown(e) {
    e.preventDefault();
    this.isDragging = true;
    this.startY = e.clientY;

    if (this.sliderState) {
      this.startValue = this.sliderState.getNormalisedValue();
      this.sliderState.sliderDragStarted();
    } else {
      this.startValue = this.currentNormalized || 0.5;
    }

    this.knob.style.cursor = 'grabbing';
  }

  onMouseMove(e) {
    if (!this.isDragging) return;

    // Calculate delta (negative because dragging up = increase)
    const deltaY = this.startY - e.clientY;
    const sensitivity = e.shiftKey ? 0.001 : 0.005; // Fine control with Shift
    const deltaValue = deltaY * sensitivity;

    let newNormalized = Math.max(0, Math.min(1, this.startValue + deltaValue));

    if (this.sliderState) {
      this.sliderState.setNormalisedValue(newNormalized);
    } else {
      // Standalone mode
      const scaled = this.config.min + newNormalized * (this.config.max - this.config.min);
      this.updateUI(newNormalized, scaled);
    }
  }

  onMouseUp() {
    if (!this.isDragging) return;
    this.isDragging = false;

    if (this.sliderState) {
      this.sliderState.sliderDragEnded();
    }

    this.knob.style.cursor = 'grab';
  }

  onDoubleClick() {
    // Reset to default
    const defaultNormalized = (this.config.default - this.config.min) / (this.config.max - this.config.min);

    if (this.sliderState) {
      this.sliderState.sliderDragStarted();
      this.sliderState.setNormalisedValue(defaultNormalized);
      this.sliderState.sliderDragEnded();
    } else {
      this.updateUI(defaultNormalized, this.config.default);
    }
  }

  updateUI(normalized, scaled) {
    this.currentNormalized = normalized;

    // Update SVG arc (stroke-dasharray)
    // At normalized=0, arc should be invisible (dasharray = "0 289.03")
    // At normalized=1, arc should be full 270° (dasharray = "216.77 289.03")
    if (this.arcEl) {
      const arcProgress = normalized * ARC_LENGTH;
      this.arcEl.style.strokeDasharray = `${arcProgress} ${ARC_CIRCUMFERENCE}`;
    }

    // Update indicator rotation
    const angle = KNOB_MIN_ANGLE + normalized * (KNOB_MAX_ANGLE - KNOB_MIN_ANGLE);
    if (this.indicatorEl) {
      // The indicator needs to rotate around the center of the knob body
      const knobBody = this.indicatorEl.parentElement;
      if (knobBody) {
        const bodyHeight = knobBody.offsetHeight;
        const pivotDistance = (bodyHeight / 2) - 4; // Center of body minus small offset
        this.indicatorEl.style.transformOrigin = `center ${pivotDistance}px`;
        this.indicatorEl.style.transform = `translateX(-50%) rotate(${angle}deg)`;
      }
    }

    // Update value display
    if (this.valueEl) {
      const displayValue = scaled.toFixed(this.config.decimals);
      if (this.paramId === 'fat' || this.paramId === 'mix') {
        this.valueEl.textContent = displayValue;
      } else if (this.paramId === 'ratio') {
        this.valueEl.textContent = `${displayValue}:1`;
      } else {
        this.valueEl.textContent = `${displayValue} ${this.config.unit}`;
      }
    }

    // Update graph display for threshold/ratio
    if (this.paramId === 'threshold') {
      const thrDisplay = document.getElementById('thrDisplay');
      if (thrDisplay) thrDisplay.textContent = `${scaled.toFixed(0)}dB`;
      updateCompressionCurve();
    } else if (this.paramId === 'ratio') {
      const ratDisplay = document.getElementById('ratDisplay');
      if (ratDisplay) ratDisplay.textContent = `${scaled.toFixed(1)}:1`;
      updateCompressionCurve();
    } else if (this.paramId === 'fat') {
      // Update FAT label glow based on knob value
      currentFatValue = normalized;
      updateFatLabelGlow();
    }
  }
}

// ============================================
// COMPRESSION CURVE GRAPH - HARD KNEE (NO CURVE)
// ============================================

function updateCompressionCurve() {
  // Get current threshold and ratio
  let threshold = -20;
  let ratio = 4;

  if (sliderStates.threshold) {
    threshold = sliderStates.threshold.getScaledValue();
  }
  if (sliderStates.ratio) {
    ratio = sliderStates.ratio.getScaledValue();
  }

  // Graph coordinates
  const graphLeft = 60;
  const graphRight = 520;
  const graphTop = 30;
  const graphBottom = 200;
  const graphWidth = graphRight - graphLeft;
  const graphHeight = graphBottom - graphTop;

  // Map dB to pixel coordinates
  // X axis: -60dB to 0dB
  // Y axis: -60dB to 0dB
  const dbToX = (db) => graphLeft + ((db + 60) / 60) * graphWidth;
  const dbToY = (db) => graphBottom - ((db + 60) / 60) * graphHeight;

  // Threshold point position (this is on the 1:1 line)
  const threshX = dbToX(threshold);
  const threshY = dbToY(threshold);

  // Update threshold point
  const thresholdPoint = document.getElementById('thresholdPoint');
  if (thresholdPoint) {
    thresholdPoint.setAttribute('cx', threshX);
    thresholdPoint.setAttribute('cy', threshY);
  }

  // Update threshold lines
  const thresholdLineV = document.getElementById('thresholdLineV');
  const thresholdLineH = document.getElementById('thresholdLineH');
  if (thresholdLineV) {
    thresholdLineV.setAttribute('x1', threshX);
    thresholdLineV.setAttribute('x2', threshX);
  }
  if (thresholdLineH) {
    thresholdLineH.setAttribute('y1', threshY);
    thresholdLineH.setAttribute('y2', threshY);
  }

  // HARD KNEE - straight line path, no curve!
  // Below threshold: 1:1 line (input = output)
  // Above threshold: compressed line with slope = 1/ratio

  const startX = graphLeft;
  const startY = graphBottom;

  // End point (0dB input)
  // Output at 0dB = threshold + (0 - threshold) / ratio
  const outputAt0 = threshold + (0 - threshold) / ratio;
  const endX = dbToX(0);
  const endY = dbToY(outputAt0);

  // Build path - STRAIGHT LINES ONLY (hard knee)
  // Line from (-60, -60) to threshold point, then from threshold to (0, output)
  const curvePath = `M ${startX} ${startY} L ${threshX} ${threshY} L ${endX} ${endY}`;
  const fillPath = `${curvePath} L ${endX} ${graphBottom} Z`;

  const compressionCurve = document.getElementById('compressionCurve');
  const compressionFill = document.getElementById('compressionFill');
  if (compressionCurve) compressionCurve.setAttribute('d', curvePath);
  if (compressionFill) compressionFill.setAttribute('d', fillPath);
}

// ============================================
// METERING (via JUCE events)
// ============================================

function setupMetering() {
  function trySetupListener() {
    if (window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.addEventListener('metering', (data) => {
        if (data) {
          updateMeters(data);
        }
      });
      console.log('[FatPressor] Metering listener registered');
    } else {
      // Retry until JUCE backend is ready
      setTimeout(trySetupListener, 100);
    }
  }
  trySetupListener();
}

function updateMeters(data) {
  const inputMeter = document.getElementById('inputMeter');
  const grMeter = document.getElementById('grMeter');
  const outputMeter = document.getElementById('outputMeter');
  const grDisplay = document.getElementById('grDisplay');

  // Convert dB to percentage (assuming -60dB to 0dB range)
  const dbToPercent = (db) => Math.max(0, Math.min(100, ((db + 60) / 60) * 100));

  if (inputMeter && typeof data.inputL === 'number') {
    const avgInput = (data.inputL + (data.inputR || data.inputL)) / 2;
    inputMeter.style.width = dbToPercent(avgInput) + '%';
  }

  if (outputMeter && typeof data.outputL === 'number') {
    const avgOutput = (data.outputL + (data.outputR || data.outputL)) / 2;
    outputMeter.style.width = dbToPercent(avgOutput) + '%';
  }

  if (grMeter && typeof data.gr === 'number') {
    // GR is positive value (0 to ~24dB)
    const grPercent = Math.min(100, (data.gr / 24) * 100);
    grMeter.style.width = grPercent + '%';
  }

  if (grDisplay && typeof data.gr === 'number') {
    grDisplay.textContent = `-${data.gr.toFixed(1)}dB`;
  }

  // Update FAT label glow based on gain reduction
  if (typeof data.gr === 'number') {
    currentGainReduction = data.gr;
    updateFatLabelGlow();
  }

  // Update compression curve glow based on output level
  if (typeof data.outputL === 'number') {
    const avgOutput = (data.outputL + (data.outputR || data.outputL)) / 2;
    currentOutputLevel = avgOutput;
    updateCurveGlow();
  }
}

// ============================================
// COMPRESSION CURVE DYNAMIC GLOW
// ============================================

function updateCurveGlow() {
  // On Windows, WebView2 breaks ALL blur filters - skip entirely
  // CSS handles brighter colors, no dynamic glow on Windows
  if (document.body.classList.contains('windows')) {
    return;
  }

  // macOS/Safari: use SVG filters (they work correctly)
  const curveBlur1 = document.getElementById('curveBlur1');
  const curveBlur2 = document.getElementById('curveBlur2');
  const pointBlur1 = document.getElementById('pointBlur1');
  const pointBlur2 = document.getElementById('pointBlur2');

  if (!curveBlur1) return;

  // Normalize output level: -60dB = 0, 0dB = 1
  const intensity = Math.max(0, Math.min(1, (currentOutputLevel + 60) / 60));

  // Curve blur - base 2, max 12
  const curveStd1 = 2 + intensity * 10;
  const curveStd2 = 4 + intensity * 16;

  // Point blur - base 4, max 20
  const pointStd1 = 4 + intensity * 12;
  const pointStd2 = 8 + intensity * 20;

  // Update SVG filter stdDeviation attributes
  curveBlur1.setAttribute('stdDeviation', curveStd1);
  curveBlur2.setAttribute('stdDeviation', curveStd2);

  if (pointBlur1 && pointBlur2) {
    pointBlur1.setAttribute('stdDeviation', pointStd1);
    pointBlur2.setAttribute('stdDeviation', pointStd2);
  }
}

// ============================================
// FAT LABEL DYNAMIC GLOW
// ============================================

function updateFatLabelGlow() {
  const fatLabel = document.querySelector('.fat-label');
  if (!fatLabel) return;

  // Combine FAT knob value (0-1) with gain reduction (0-24dB normalized to 0-1)
  const fatNormalized = currentFatValue; // 0 to 1
  const grNormalized = Math.min(1, currentGainReduction / 12); // 0 to 1 (12dB = full)

  // Combined intensity: FAT provides base, GR adds reactivity
  const baseIntensity = fatNormalized;
  const reactiveBoost = grNormalized * fatNormalized * 0.5; // GR only affects when FAT is up
  const intensity = Math.min(1, baseIntensity + reactiveBoost);

  // Color interpolation: dull yellow (#8a7a50) to bright orange (#ff9500)
  const r = Math.round(138 + (255 - 138) * intensity);
  const g = Math.round(122 + (149 - 122) * intensity);
  const b = Math.round(80 + (0 - 80) * intensity);
  const color = `rgb(${r}, ${g}, ${b})`;

  // Glow intensity
  const glowSize1 = Math.round(intensity * 10);
  const glowSize2 = Math.round(intensity * 20);
  const glowSize3 = Math.round(intensity * 40);
  const glowSize4 = Math.round(intensity * 60);
  const glowOpacity = intensity * 0.8;

  if (intensity < 0.1) {
    fatLabel.style.color = '#8a7a50';
    fatLabel.style.textShadow = 'none';
  } else {
    fatLabel.style.color = color;
    fatLabel.style.textShadow = `
      0 0 ${glowSize1}px rgba(255, 149, 0, ${glowOpacity}),
      0 0 ${glowSize2}px rgba(255, 149, 0, ${glowOpacity * 0.8}),
      0 0 ${glowSize3}px rgba(255, 149, 0, ${glowOpacity * 0.5}),
      0 0 ${glowSize4}px rgba(255, 149, 0, ${glowOpacity * 0.3})
    `;
  }
}

// ============================================
// THRESHOLD POINT DRAG (on graph)
// ============================================

function setupThresholdPointDrag() {
  const thresholdPoint = document.getElementById('thresholdPoint');
  if (!thresholdPoint) return;

  let isDragging = false;
  let startY = 0;
  let startThreshold = -20;

  thresholdPoint.addEventListener('mousedown', (e) => {
    e.preventDefault();
    e.stopPropagation();
    isDragging = true;
    startY = e.clientY;

    if (sliderStates.threshold) {
      startThreshold = sliderStates.threshold.getScaledValue();
      sliderStates.threshold.sliderDragStarted();
    }

    thresholdPoint.classList.add('dragging');
  });

  document.addEventListener('mousemove', (e) => {
    if (!isDragging) return;

    // Calculate new threshold based on Y movement
    const deltaY = startY - e.clientY;
    const sensitivity = e.shiftKey ? 0.2 : 1; // Fine control with Shift
    const newThreshold = Math.max(-60, Math.min(0, startThreshold + deltaY * sensitivity));

    if (sliderStates.threshold) {
      const normalized = (newThreshold - (-60)) / (0 - (-60));
      sliderStates.threshold.setNormalisedValue(normalized);
    }
  });

  document.addEventListener('mouseup', () => {
    if (isDragging) {
      isDragging = false;
      if (sliderStates.threshold) {
        sliderStates.threshold.sliderDragEnded();
      }
      thresholdPoint.classList.remove('dragging');
    }
  });
}

// ============================================
// PRESET BROWSER
// ============================================

const PRESET_CATEGORIES = ['All', 'Drums', 'Vocals', 'Bass', 'Mix Bus', 'Uncategorized'];

let currentPresets = [];
let currentPresetIndex = -1;
let selectedCategory = 'All';
let presetBrowserOpen = false;
let pendingDeleteIndex = -1;  // Track preset pending deletion

function setupPresetBrowser() {
  const presetName = document.getElementById('presetName');
  const presetBrowser = document.getElementById('presetBrowser');
  const presetPrev = document.getElementById('presetPrev');
  const presetNext = document.getElementById('presetNext');
  const presetCategories = document.getElementById('presetCategories');
  const presetSaveBtn = document.getElementById('presetSaveBtn');

  if (!presetName || !presetBrowser) return;

  // Toggle browser on preset name click
  presetName.addEventListener('click', (e) => {
    e.stopPropagation();
    presetBrowserOpen = !presetBrowserOpen;
    presetName.classList.toggle('open', presetBrowserOpen);
    presetBrowser.classList.toggle('open', presetBrowserOpen);
    if (presetBrowserOpen) {
      refreshPresetList();
    }
  });

  // Close browser on outside click
  document.addEventListener('click', (e) => {
    if (presetBrowserOpen && !presetBrowser.contains(e.target) && !presetName.contains(e.target)) {
      presetBrowserOpen = false;
      presetName.classList.remove('open');
      presetBrowser.classList.remove('open');
    }
  });

  // Prev/Next buttons
  if (presetPrev) {
    presetPrev.addEventListener('click', (e) => {
      e.stopPropagation();
      loadPreviousPreset();
    });
  }

  if (presetNext) {
    presetNext.addEventListener('click', (e) => {
      e.stopPropagation();
      loadNextPreset();
    });
  }

  // Populate categories
  if (presetCategories) {
    presetCategories.innerHTML = PRESET_CATEGORIES.map(cat =>
      `<button class="preset-category-btn${cat === 'All' ? ' active' : ''}" data-category="${cat}">${cat}</button>`
    ).join('');

    presetCategories.addEventListener('click', (e) => {
      const btn = e.target.closest('.preset-category-btn');
      if (btn) {
        selectedCategory = btn.dataset.category;
        presetCategories.querySelectorAll('.preset-category-btn').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        refreshPresetList();
      }
    });
  }

  // Save button (now outside dropdown)
  const presetSaveModal = document.getElementById('presetSaveModal');
  const presetSaveCancel = document.getElementById('presetSaveCancel');
  const presetSaveConfirm = document.getElementById('presetSaveConfirm');

  if (presetSaveBtn && presetSaveModal) {
    presetSaveBtn.addEventListener('click', (e) => {
      e.stopPropagation();
      presetSaveModal.classList.add('open');
      document.getElementById('presetSaveName').value = '';
      document.getElementById('presetSaveName').focus();
      // Reset category dropdown
      resetCategoryDropdown();
    });

    presetSaveCancel.addEventListener('click', () => {
      presetSaveModal.classList.remove('open');
      closeCategoryDropdown();
    });

    presetSaveConfirm.addEventListener('click', () => {
      const name = document.getElementById('presetSaveName').value.trim();
      const category = document.getElementById('presetSaveCategory').value;
      if (name) {
        saveUserPreset(name, category);
        presetSaveModal.classList.remove('open');
        closeCategoryDropdown();
      }
    });

    // Close modal on backdrop click
    presetSaveModal.addEventListener('click', (e) => {
      if (e.target === presetSaveModal) {
        presetSaveModal.classList.remove('open');
        closeCategoryDropdown();
      }
    });
  }

  // Setup custom category dropdown
  setupCategoryDropdown();

  // Load presets immediately so arrows work
  useFallbackPresets();

  // Then try to get real presets from JUCE
  requestPresetList();
}

function refreshPresetList() {
  const presetList = document.getElementById('presetList');
  if (!presetList) return;

  const filteredPresets = selectedCategory === 'All'
    ? currentPresets
    : currentPresets.filter(p => p.category === selectedCategory);

  presetList.innerHTML = filteredPresets.map((preset, idx) => {
    const globalIdx = currentPresets.indexOf(preset);
    const isActive = globalIdx === currentPresetIndex;
    const iconClass = preset.isFactory ? 'factory' : 'user';
    const icon = preset.isFactory ? '★' : '♦';
    const deleteBtn = preset.isFactory ? '' : `<span class="preset-item-delete" data-delete-index="${globalIdx}" title="Delete preset">✕</span>`;

    return `
      <div class="preset-item${isActive ? ' active' : ''}" data-index="${globalIdx}">
        <span class="preset-item-icon ${iconClass}">${icon}</span>
        <span class="preset-item-name">${preset.name}</span>
        <span class="preset-item-category">${preset.category}</span>
        ${deleteBtn}
      </div>
    `;
  }).join('');

  // Add click handlers for preset selection
  presetList.querySelectorAll('.preset-item').forEach(item => {
    item.addEventListener('click', (e) => {
      // Don't select if clicking delete button
      if (e.target.classList.contains('preset-item-delete')) return;
      const idx = parseInt(item.dataset.index);
      loadPresetByIndex(idx);
    });
  });

  // Add click handlers for delete buttons
  presetList.querySelectorAll('.preset-item-delete').forEach(btn => {
    btn.addEventListener('click', (e) => {
      e.stopPropagation();
      e.preventDefault();
      const idx = parseInt(btn.dataset.deleteIndex);
      const preset = currentPresets[idx];
      if (preset && !preset.isFactory) {
        showDeleteModal(idx, preset.name);
      }
    });
  });
}

function updatePresetDisplay(name) {
  const presetNameText = document.getElementById('presetNameText');
  if (presetNameText) {
    presetNameText.textContent = name || 'Default';
  }
}

// ============================================
// CUSTOM CATEGORY DROPDOWN
// ============================================

function setupCategoryDropdown() {
  const dropdown = document.getElementById('categoryDropdown');
  const selected = document.getElementById('categorySelected');
  const options = document.getElementById('categoryOptions');
  const hiddenInput = document.getElementById('presetSaveCategory');

  if (!dropdown || !selected || !options) return;

  // Toggle dropdown on click
  selected.addEventListener('click', (e) => {
    e.stopPropagation();
    dropdown.classList.toggle('open');
  });

  // Handle option selection
  options.addEventListener('click', (e) => {
    const option = e.target.closest('.custom-dropdown-option');
    if (option) {
      const value = option.dataset.value;
      const text = option.textContent;

      // Update selected text
      selected.querySelector('.custom-dropdown-text').textContent = text;

      // Update hidden input
      if (hiddenInput) hiddenInput.value = value;

      // Update active state
      options.querySelectorAll('.custom-dropdown-option').forEach(opt => opt.classList.remove('active'));
      option.classList.add('active');

      // Close dropdown
      dropdown.classList.remove('open');
    }
  });

  // Close dropdown when clicking outside
  document.addEventListener('click', (e) => {
    if (!dropdown.contains(e.target)) {
      dropdown.classList.remove('open');
    }
  });
}

function closeCategoryDropdown() {
  const dropdown = document.getElementById('categoryDropdown');
  if (dropdown) dropdown.classList.remove('open');
}

function resetCategoryDropdown() {
  const selected = document.getElementById('categorySelected');
  const options = document.getElementById('categoryOptions');
  const hiddenInput = document.getElementById('presetSaveCategory');

  if (selected) {
    selected.querySelector('.custom-dropdown-text').textContent = 'Uncategorized';
  }
  if (hiddenInput) {
    hiddenInput.value = 'Uncategorized';
  }
  if (options) {
    options.querySelectorAll('.custom-dropdown-option').forEach(opt => {
      opt.classList.toggle('active', opt.dataset.value === 'Uncategorized');
    });
  }
  closeCategoryDropdown();
}

// ============================================
// DELETE PRESET MODAL
// ============================================

function setupDeleteModal() {
  const modal = document.getElementById('presetDeleteModal');
  const cancelBtn = document.getElementById('presetDeleteCancel');
  const confirmBtn = document.getElementById('presetDeleteConfirm');

  if (!modal || !cancelBtn || !confirmBtn) return;

  cancelBtn.addEventListener('click', () => hideDeleteModal());

  confirmBtn.addEventListener('click', () => {
    if (pendingDeleteIndex >= 0) {
      deleteUserPreset(pendingDeleteIndex);
    }
    hideDeleteModal();
  });

  // Close on backdrop click
  modal.addEventListener('click', (e) => {
    if (e.target === modal) hideDeleteModal();
  });
}

function showDeleteModal(index, presetName) {
  const modal = document.getElementById('presetDeleteModal');
  const nameSpan = document.getElementById('presetDeleteName');
  if (!modal) return;

  pendingDeleteIndex = index;
  if (nameSpan) nameSpan.textContent = presetName;
  modal.classList.add('open');
}

function hideDeleteModal() {
  const modal = document.getElementById('presetDeleteModal');
  if (modal) {
    modal.classList.remove('open');
  }
  pendingDeleteIndex = -1;
}

// ============================================
// PRESET NATIVE BRIDGE
// ============================================

async function requestPresetList() {
  console.log('[FatPressor]', 'requestPresetList() called');

  try {
    console.log('[FatPressor]', 'Calling native getPresetList...');
    const nativeGetList = getNativeFunction('getPresetList');
    const result = await nativeGetList();
    console.log('[FatPressor]', 'Native getPresetList returned: ' + JSON.stringify(result));
    if (result && result.presets) {
      currentPresets = result.presets;
      currentPresetIndex = result.currentIndex || 0;
      if (currentPresets.length > 0) {
        updatePresetDisplay(currentPresets[currentPresetIndex].name);
      }
      refreshPresetList();
      console.log('[FatPressor]', 'Loaded ' + currentPresets.length + ' presets from C++');
      return;
    }
  } catch (e) {
    console.error('[FatPressor]', 'getPresetList failed: ' + e);
  }
  // Fallback if native function not available
  useFallbackPresets();
}

function useFallbackPresets() {
  // Fallback presets for standalone/testing
  currentPresets = [
    { name: 'Punchy Kick', category: 'Drums', isFactory: true },
    { name: 'Snare Snap', category: 'Drums', isFactory: true },
    { name: 'Room Glue', category: 'Drums', isFactory: true },
    { name: 'Parallel Smash', category: 'Drums', isFactory: true },
    { name: 'Drum Bus', category: 'Drums', isFactory: true },
    { name: 'Gentle Lead', category: 'Vocals', isFactory: true },
    { name: 'Radio Ready', category: 'Vocals', isFactory: true },
    { name: 'Intimate', category: 'Vocals', isFactory: true },
    { name: 'De-Harsh', category: 'Vocals', isFactory: true },
    { name: 'Background Vox', category: 'Vocals', isFactory: true },
    { name: 'Tight Low', category: 'Bass', isFactory: true },
    { name: 'Tube Warmth', category: 'Bass', isFactory: true },
    { name: 'Slap Bass', category: 'Bass', isFactory: true },
    { name: 'Sub Control', category: 'Bass', isFactory: true },
    { name: 'Vintage Bass', category: 'Bass', isFactory: true },
    { name: 'Glue Master', category: 'Mix Bus', isFactory: true },
    { name: 'Loud & Proud', category: 'Mix Bus', isFactory: true },
    { name: 'Transparent', category: 'Mix Bus', isFactory: true },
    { name: 'Analog Sum', category: 'Mix Bus', isFactory: true },
    { name: 'Final Touch', category: 'Mix Bus', isFactory: true },
  ];
  currentPresetIndex = 0;
  updatePresetDisplay(currentPresets[0].name);
  refreshPresetList();
}

async function loadPresetByIndex(index) {
  console.log('[FatPressor]', 'loadPresetByIndex(' + index + ') called');
  if (index < 0 || index >= currentPresets.length) return;

  try {
    console.log('[FatPressor]', 'Calling native loadPresetByIndex(' + index + ')...');
    const nativeLoad = getNativeFunction('loadPresetByIndex');
    await nativeLoad(index);
    console.log('[FatPressor]', 'Native loadPresetByIndex completed');
    // C++ will send presetChanged event, which updates UI
  } catch (e) {
    console.error('[FatPressor]', 'loadPresetByIndex failed: ' + e);
    // Fallback: just update UI
    currentPresetIndex = index;
    updatePresetDisplay(currentPresets[index].name);
  }

  // Close browser
  const presetName = document.getElementById('presetName');
  const presetBrowser = document.getElementById('presetBrowser');
  presetBrowserOpen = false;
  if (presetName) presetName.classList.remove('open');
  if (presetBrowser) presetBrowser.classList.remove('open');

  refreshPresetList();
}

async function loadNextPreset() {
  console.log('[FatPressor]', 'loadNextPreset() called');
  if (currentPresets.length === 0) return;

  try {
    console.log('[FatPressor]', 'Calling native loadNextPreset...');
    const nativeLoadNext = getNativeFunction('loadNextPreset');
    await nativeLoadNext();
    console.log('[FatPressor]', 'Native loadNextPreset completed');
    // C++ will send presetChanged event
  } catch (e) {
    console.error('[FatPressor]', 'loadNextPreset failed: ' + e);
    const nextIndex = (currentPresetIndex + 1) % currentPresets.length;
    currentPresetIndex = nextIndex;
    updatePresetDisplay(currentPresets[nextIndex].name);
    refreshPresetList();
  }
}

async function loadPreviousPreset() {
  console.log('[FatPressor]', 'loadPreviousPreset() called');
  if (currentPresets.length === 0) return;

  try {
    console.log('[FatPressor]', 'Calling native loadPreviousPreset...');
    const nativeLoadPrev = getNativeFunction('loadPreviousPreset');
    await nativeLoadPrev();
    console.log('[FatPressor]', 'Native loadPreviousPreset completed');
    // C++ will send presetChanged event
  } catch (e) {
    console.error('[FatPressor]', 'loadPreviousPreset failed: ' + e);
    let prevIndex = currentPresetIndex - 1;
    if (prevIndex < 0) prevIndex = currentPresets.length - 1;
    currentPresetIndex = prevIndex;
    updatePresetDisplay(currentPresets[prevIndex].name);
    refreshPresetList();
  }
}

async function saveUserPreset(name, category) {
  console.log('[FatPressor]', 'saveUserPreset("' + name + '", "' + category + '") called');

  try {
    console.log('[FatPressor]', 'Calling native saveUserPreset...');
    const nativeSave = getNativeFunction('saveUserPreset');
    const success = await nativeSave(name, category);
    console.log('[FatPressor]', 'Native saveUserPreset returned: ' + success);
    // C++ will send presetListChanged event
  } catch (e) {
    console.error('[FatPressor]', 'saveUserPreset failed: ' + e);
    currentPresets.push({ name: name, category: category, isFactory: false });
    currentPresetIndex = currentPresets.length - 1;
    updatePresetDisplay(name);
    refreshPresetList();
  }
}

async function deleteUserPreset(index) {
  const preset = currentPresets[index];
  if (!preset || preset.isFactory) return;

  try {
    const nativeDelete = getNativeFunction('deleteUserPreset');
    const success = await nativeDelete(index);
    if (success) {
      requestPresetList();
    }
  } catch (e) {
    console.error('[FatPressor] deleteUserPreset failed:', e);
    // Fallback: remove from local list
    currentPresets.splice(index, 1);
    if (currentPresetIndex >= currentPresets.length) {
      currentPresetIndex = Math.max(0, currentPresets.length - 1);
    }
    if (currentPresets.length > 0) {
      updatePresetDisplay(currentPresets[currentPresetIndex].name);
    }
    refreshPresetList();
  }
}

// Listen for preset events from C++
function setupPresetListeners() {
  function trySetupListeners() {
    if (window.__JUCE__ && window.__JUCE__.backend) {
      // Preset list received
      window.__JUCE__.backend.addEventListener('presetList', (data) => {
        console.log('[FatPressor] Received presetList event:', data);
        if (data && Array.isArray(data.presets)) {
          currentPresets = data.presets;
          currentPresetIndex = data.currentIndex || 0;
          if (currentPresets.length > 0) {
            updatePresetDisplay(currentPresets[currentPresetIndex].name);
          }
          refreshPresetList();
        }
      });

      // Preset changed (from DAW or prev/next)
      window.__JUCE__.backend.addEventListener('presetChanged', (data) => {
        console.log('[FatPressor:event]', 'presetChanged: ' + JSON.stringify(data));
        if (data) {
          currentPresetIndex = data.index || 0;
          updatePresetDisplay(data.name || 'Default');
          refreshPresetList();
        }
      });

      // Parameter sync - update all knobs after preset load
      window.__JUCE__.backend.addEventListener('parameterSync', (data) => {
        console.log('[FatPressor:event]', 'parameterSync: ' + JSON.stringify(data));
        if (data) {
          syncKnobsFromParams(data);
        }
      });

      console.log('[FatPressor]', 'Preset listeners registered');
    } else {
      // Retry until JUCE backend is ready
      setTimeout(trySetupListeners, 100);
    }
  }
  trySetupListeners();
}

// Update all knob UIs from parameter values
function syncKnobsFromParams(params) {
  console.log('[FatPressor] syncKnobsFromParams called with:', JSON.stringify(params));
  const knobIds = ['threshold', 'ratio', 'attack', 'release', 'fat', 'output', 'mix'];

  knobIds.forEach(id => {
    const normalizedKey = id + '_normalized';
    const scaledKey = id + '_scaled';

    console.log('[FatPressor] Checking ' + id + ':', normalizedKey, '=', params[normalizedKey], scaledKey, '=', params[scaledKey]);

    if (params[normalizedKey] !== undefined && params[scaledKey] !== undefined) {
      const normalized = params[normalizedKey];
      const scaled = params[scaledKey];
      const config = PARAMS[id];
      console.log('[FatPressor] Updating ' + id + ': normalized=' + normalized + ', scaled=' + scaled);

      // Update arc
      const arcEl = document.getElementById(`${id}Arc`);
      if (arcEl) {
        const arcProgress = normalized * ARC_LENGTH;
        arcEl.style.strokeDasharray = `${arcProgress} ${ARC_CIRCUMFERENCE}`;
      }

      // Update indicator rotation
      const indicatorEl = document.getElementById(`${id}Indicator`);
      if (indicatorEl) {
        const angle = KNOB_MIN_ANGLE + normalized * (KNOB_MAX_ANGLE - KNOB_MIN_ANGLE);
        const knobBody = indicatorEl.parentElement;
        if (knobBody) {
          const bodyHeight = knobBody.offsetHeight;
          const pivotDistance = (bodyHeight / 2) - 4;
          indicatorEl.style.transformOrigin = `center ${pivotDistance}px`;
          indicatorEl.style.transform = `translateX(-50%) rotate(${angle}deg)`;
        }
      }

      // Update value display
      const valueEl = document.getElementById(`${id}Value`);
      if (valueEl && config) {
        const displayValue = scaled.toFixed(config.decimals);
        if (id === 'fat' || id === 'mix') {
          valueEl.textContent = displayValue;
        } else if (id === 'ratio') {
          valueEl.textContent = `${displayValue}:1`;
        } else {
          valueEl.textContent = `${displayValue} ${config.unit}`;
        }
      }

      // Update graph displays for threshold/ratio
      if (id === 'threshold') {
        const thrDisplay = document.getElementById('thrDisplay');
        if (thrDisplay) thrDisplay.textContent = `${scaled.toFixed(0)}dB`;
      } else if (id === 'ratio') {
        const ratDisplay = document.getElementById('ratDisplay');
        if (ratDisplay) ratDisplay.textContent = `${scaled.toFixed(1)}:1`;
      } else if (id === 'fat') {
        currentFatValue = normalized;
        updateFatLabelGlow();
      }
    }
  });

  // Update compression curve with the synced values
  const threshold = params['threshold_scaled'] !== undefined ? params['threshold_scaled'] : -20;
  const ratio = params['ratio_scaled'] !== undefined ? params['ratio_scaled'] : 4;
  updateCompressionCurveWithValues(threshold, ratio);
}

// Update compression curve with explicit values (used after preset sync)
function updateCompressionCurveWithValues(threshold, ratio) {
  // Graph coordinates
  const graphLeft = 60;
  const graphRight = 520;
  const graphTop = 30;
  const graphBottom = 200;
  const graphWidth = graphRight - graphLeft;
  const graphHeight = graphBottom - graphTop;

  // Map dB to pixel coordinates
  const dbToX = (db) => graphLeft + ((db + 60) / 60) * graphWidth;
  const dbToY = (db) => graphBottom - ((db + 60) / 60) * graphHeight;

  // Threshold point position
  const threshX = dbToX(threshold);
  const threshY = dbToY(threshold);

  // Update threshold point
  const thresholdPoint = document.getElementById('thresholdPoint');
  if (thresholdPoint) {
    thresholdPoint.setAttribute('cx', threshX);
    thresholdPoint.setAttribute('cy', threshY);
  }

  // Update threshold lines
  const thresholdLineV = document.getElementById('thresholdLineV');
  const thresholdLineH = document.getElementById('thresholdLineH');
  if (thresholdLineV) {
    thresholdLineV.setAttribute('x1', threshX);
    thresholdLineV.setAttribute('x2', threshX);
  }
  if (thresholdLineH) {
    thresholdLineH.setAttribute('y1', threshY);
    thresholdLineH.setAttribute('y2', threshY);
  }

  // Build path
  const startX = graphLeft;
  const startY = graphBottom;
  const outputAt0 = threshold + (0 - threshold) / ratio;
  const endX = dbToX(0);
  const endY = dbToY(outputAt0);

  const curvePath = `M ${startX} ${startY} L ${threshX} ${threshY} L ${endX} ${endY}`;
  const fillPath = `${curvePath} L ${endX} ${graphBottom} Z`;

  const compressionCurve = document.getElementById('compressionCurve');
  const compressionFill = document.getElementById('compressionFill');
  if (compressionCurve) compressionCurve.setAttribute('d', curvePath);
  if (compressionFill) compressionFill.setAttribute('d', fillPath);
}

// ============================================
// CREDITS OVERLAY
// ============================================

function setupCreditsOverlay() {
  const logoBtn = document.getElementById('logoBtn');
  const creditsOverlay = document.getElementById('creditsOverlay');
  const creditsClose = document.getElementById('creditsClose');
  const creditsBackdrop = creditsOverlay?.querySelector('.credits-backdrop');

  if (!logoBtn || !creditsOverlay) return;

  logoBtn.addEventListener('click', () => {
    creditsOverlay.classList.add('open');
  });

  creditsClose?.addEventListener('click', () => {
    creditsOverlay.classList.remove('open');
  });

  creditsBackdrop?.addEventListener('click', () => {
    creditsOverlay.classList.remove('open');
  });

  // Close on Escape key
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape' && creditsOverlay.classList.contains('open')) {
      creditsOverlay.classList.remove('open');
    }
  });
}

// ============================================
// WINDOWS PLATFORM DETECTION
// ============================================

function detectWindowsPlatform() {
  // Detect Windows via User-Agent
  const isWindows = navigator.userAgent.includes('Windows');

  if (isWindows) {
    console.log('[FatPressor] Windows detected - using CSS glow fallback');
    document.body.classList.add('windows');

    // Remove SVG filter attributes (they render as squares on WebView2)
    // The CSS will handle glow via drop-shadow instead
    const elementsWithFilters = document.querySelectorAll('[filter]');
    elementsWithFilters.forEach(el => {
      el.removeAttribute('filter');
    });
  }
}

// ============================================
// INITIALIZATION
// ============================================

document.addEventListener('DOMContentLoaded', () => {
  // Detect Windows and apply CSS fallback for glow effects
  detectWindowsPlatform();

  // Setup credits overlay
  setupCreditsOverlay();

  // Disable right-click context menu
  document.addEventListener('contextmenu', (e) => {
    e.preventDefault();
    return false;
  });

  // Initialize knob controls
  const knobs = [
    { id: 'fatKnob', param: 'fat' },
    { id: 'thresholdKnob', param: 'threshold' },
    { id: 'ratioKnob', param: 'ratio' },
    { id: 'attackKnob', param: 'attack' },
    { id: 'releaseKnob', param: 'release' },
    { id: 'outputKnob', param: 'output' },
    { id: 'mixKnob', param: 'mix' }
  ];

  knobs.forEach(({ id, param }) => {
    const el = document.getElementById(id);
    if (el && PARAMS[param]) {
      new KnobControl(el, param, PARAMS[param]);
    }
  });

  // Setup threshold point dragging
  setupThresholdPointDrag();

  // Setup metering
  setupMetering();

  // Setup preset browser
  setupPresetBrowser();
  setupDeleteModal();
  setupPresetListeners();

  // Initial curve update
  setTimeout(updateCompressionCurve, 100);

  console.log('[FatPressor] UI initialized');
});
