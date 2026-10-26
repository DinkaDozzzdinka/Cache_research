set terminal svg size 1200,650 dynamic
set output 'plots/global_top10.svg'
set xrange [0:100]
set yrange [9.5:-0.5]
set grid xtics
set xlabel 'Macro-average hit ratio (%)'
unset ylabel
set ytics nomirror
set lmargin 24
set rmargin 4
set bmargin 4
set title 'Top 10 configurations selected by training score'
set key outside top center horizontal
plot 'plot_data/global_top10.tsv' using 2:0:ytic(1) with points pt 7 ps 1.6 title 'Train', '' using 3:0 with points pt 5 ps 1.6 title 'Validation'
unset output

set terminal pngcairo size 1200,650
set output 'plots/global_top10.png'
set xrange [0:100]
set yrange [9.5:-0.5]
set grid xtics
set xlabel 'Macro-average hit ratio (%)'
unset ylabel
set ytics nomirror
set lmargin 24
set rmargin 4
set bmargin 4
set title 'Top 10 configurations selected by training score'
set key outside top center horizontal
plot 'plot_data/global_top10.tsv' using 2:0:ytic(1) with points pt 7 ps 1.6 title 'Train', '' using 3:0 with points pt 5 ps 1.6 title 'Validation'
unset output
