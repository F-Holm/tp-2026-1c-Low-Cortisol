#!/bin/bash

SESSION_NAME="low_cortisol"
COLUMNAS=4
FILAS=3

if ! command -v tmux &> /dev/null; then
    echo "Error: tmux no está instalado."
    exit 1
fi

if tmux has-session -t "$SESSION_NAME" 2>/dev/null; then
    echo "Ya existe una sesión llamada '$SESSION_NAME'. Conectando..."
    tmux attach-session -t "$SESSION_NAME"
    exit 0
fi

tmux new-session -d -s "$SESSION_NAME"

base_pane="$SESSION_NAME.0"

for ((i = 1; i <= FILAS - 1; i++)); do
    restantes=$((FILAS - i + 1))
    porcentaje=$((100 / restantes))
    tmux split-window -v -p "$porcentaje" -t "$base_pane"
done

mapfile -t filas_panes < <(tmux list-panes -t "$SESSION_NAME" -F '#{pane_id}')

for fila_pane in "${filas_panes[@]}"; do
    for ((j = 1; j <= COLUMNAS - 1; j++)); do
        restantes=$((COLUMNAS - j + 1))
        porcentaje=$((100 / restantes))
        tmux split-window -h -p "$porcentaje" -t "$fila_pane"
    done
done

tmux attach-session -t "$SESSION_NAME"
