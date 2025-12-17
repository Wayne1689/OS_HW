#!/bin/bash

# ========================================================
# 🟢 參數設定區 (老師的數值已填入)
# ========================================================
TEACHER_X=14.7829   # 老師的單執行緒時間 (X)
TEACHER_Y=4.0808    # 老師的多執行緒時間 (Y)

EXE_SINGLE="./OS_single"     # 你的單執行緒程式
EXE_MULTI="./OS_modified"    # 你的多執行緒程式

SOURCE_PREFIX="input"        # 圖片前綴
TARGET_PREFIX="output"       # 輸出前綴
AMOUNT=20                    # 圖片數量
RUNS=10                      # 總共跑幾次

# ========================================================
# 🚀 執行邏輯
# ========================================================

# 檢查程式是否存在
if [ ! -f "$EXE_SINGLE" ] || [ ! -f "$EXE_MULTI" ]; then
    echo "❌ 錯誤: 找不到執行檔 $EXE_SINGLE 或 $EXE_MULTI"
    exit 1
fi

echo ""
echo "=========================================================================================="
echo "📊 作業成績計算器 (基於加速比 Speedup)"
echo "   老師基準 X (Single): $TEACHER_X 秒"
echo "   老師基準 Y (Multi) : $TEACHER_Y 秒"
echo "=========================================================================================="
# 印出標題列
printf "%-6s | %-10s | %-10s | %-8s | %-10s | %-s\n" "Run" "Single(a)" "Multi(b)" "Speedup" "Est(Y_est)" "Grade"
echo "------------------------------------------------------------------------------------------"

for i in $(seq 1 $RUNS); do

    # 1. 執行單執行緒 (a)
    TIME_OUTPUT_A=$( { time -p "$EXE_SINGLE" "$SOURCE_PREFIX" "$TARGET_PREFIX" "$AMOUNT" > /dev/null 2>&1 ; } 2>&1 )
    TIME_A=$(echo "$TIME_OUTPUT_A" | grep real | awk '{print $2}')

    # 2. 執行多執行緒 (b)
    TIME_OUTPUT_B=$( { time -p "$EXE_MULTI" "$SOURCE_PREFIX" "$TARGET_PREFIX" "$AMOUNT" > /dev/null 2>&1 ; } 2>&1 )
    TIME_B=$(echo "$TIME_OUTPUT_B" | grep real | awk '{print $2}')

    # 3. 計算成績
    awk -v x="$TEACHER_X" \
        -v y="$TEACHER_Y" \
        -v a="$TIME_A" \
        -v b="$TIME_B" \
        -v run="$i" '
    BEGIN {
        # 計算加速比
        if (b == 0 || a == 0) { speedup = 0; y_est = 9999; } 
        else { speedup = a / b; y_est = x / speedup; }

        # 計算分數邏輯
        grade = 0;
        if (y_est > x) {
            grade = 40; # 太慢
        } else if (y_est < y) {
            grade = (y - y_est) + 100; # 加分區
        } else {
            # 正常區間: ceil( (X-Y_est)/(X-Y) * 60 ) + 40
            score = ((x - y_est) / (x - y)) * 60;
            grade = int(score);
            if (score > grade) grade += 1; # 無條件進位
            grade += 40;
        }

        # 輸出結果 (成績使用綠色高亮)
        printf "Run %-2d | %-10.4f | %-10.4f | %-8.4f | %-10.4f | \033[1;32m%.2f\033[0m\n", run, a, b, speedup, y_est, grade;
    }'
done

echo "=========================================================================================="
echo "註: Est(Y_est) = Teacher_X / Speedup"
echo ""
