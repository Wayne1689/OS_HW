#!/bin/bash

# ========================================================
# 🟢 參數設定區 (已更新路徑)
# ========================================================
TEACHER_X=14.7829   # 老師的單執行緒時間 (X)
TEACHER_Y=4.0808    # 老師的多執行緒時間 (Y)

EXE_SINGLE="./OS_single"     # 你的單執行緒程式
EXE_MULTI="./OS_modified"    # 你的多執行緒程式

# --- 路徑修改處 ---
ASSETS_DIR="assets"          # 資料夾名稱
SOURCE_PREFIX="$ASSETS_DIR/sample"  # 會變成 assets/input
TARGET_PREFIX="$ASSETS_DIR/output" # 會變成 assets/output
# -----------------

AMOUNT=20                    # 圖片數量
RUNS=10                      # 總共跑幾次

# ========================================================
# 🚀 執行邏輯
# ========================================================

# 1. 檢查程式是否存在
if [ ! -f "$EXE_SINGLE" ] || [ ! -f "$EXE_MULTI" ]; then
    echo "❌ 錯誤: 找不到執行檔 $EXE_SINGLE 或 $EXE_MULTI"
    exit 1
fi

# 2. 檢查 assets 資料夾是否存在，不存在就建立 (避免存取失敗)
if [ ! -d "$ASSETS_DIR" ]; then
    echo "📂 找不到 $ASSETS_DIR 資料夾，正在為您建立..."
    mkdir -p "$ASSETS_DIR"
fi

echo ""
echo "=========================================================================================="
echo "📊 作業成績計算器 (資料夾: $ASSETS_DIR)"
echo "   老師基準 X (Single): $TEACHER_X 秒"
echo "   老師基準 Y (Multi) : $TEACHER_Y 秒"
echo "=========================================================================================="
# 印出標題列
printf "%-6s | %-10s | %-10s | %-8s | %-10s | %-s\n" "Run" "Single(a)" "Multi(b)" "Speedup" "Est(Y_est)" "Grade"
echo "------------------------------------------------------------------------------------------"

for i in $(seq 1 $RUNS); do

    # 執行單執行緒 (a)
    TIME_OUTPUT_A=$( { time -p "$EXE_SINGLE" "$SOURCE_PREFIX" "$TARGET_PREFIX" "$AMOUNT" > /dev/null 2>&1 ; } 2>&1 )
    TIME_A=$(echo "$TIME_OUTPUT_A" | grep real | awk '{print $2}')

    # 執行多執行緒 (b)
    TIME_OUTPUT_B=$( { time -p "$EXE_MULTI" "$SOURCE_PREFIX" "$TARGET_PREFIX" "$AMOUNT" > /dev/null 2>&1 ; } 2>&1 )
    TIME_B=$(echo "$TIME_OUTPUT_B" | grep real | awk '{print $2}')

    # 計算成績 (邏輯保持不變)
    awk -v x="$TEACHER_X" \
        -v y="$TEACHER_Y" \
        -v a="$TIME_A" \
        -v b="$TIME_B" \
        -v run="$i" '
    BEGIN {
        if (b == 0 || a == 0) { speedup = 0; y_est = 9999; } 
        else { speedup = a / b; y_est = x / speedup; }

        grade = 0;
        if (y_est > x) {
            grade = 40;
        } else if (y_est < y) {
            grade = (y - y_est) + 100;
        } else {
            score = ((x - y_est) / (x - y)) * 60;
            grade = int(score);
            if (score > grade) grade += 1;
            grade += 40;
        }

        printf "Run %-2d | %-10.4f | %-10.4f | %-8.4f | %-10.4f | \033[1;32m%.2f\033[0m\n", run, a, b, speedup, y_est, grade;
    }'
done

echo "=========================================================================================="
echo "註: Est(Y_est) = Teacher_X / Speedup"
echo ""
