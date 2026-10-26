set terminal svg size 1300,600 dynamic
set output 'plots/hot_set.svg'
set encoding utf8
set grid ytics
set yrange [0:100]
set xlabel 'Total capacity (pages)'
set ylabel 'Hit ratio (%)'
set xtics ("8" 8,"16" 16,"32" 32)
set xrange [7:33]
set key outside bottom center horizontal maxrows 3
set multiplot layout 1,2 title 'hot set'
set title 'train'
plot for [col=2:6] 'plot_data/hot_set_train.tsv' using 1:col with linespoints lw 2 title columnhead(col), 'plot_data/hot_set_train.tsv' using 1:7 with linespoints lw 3 dt 2 title 'Selected on train', 'plot_data/hot_set_train.tsv' using 1:8 with linespoints lw 2 dt 3 lc rgb 'black' title 'Ideal'
set title 'validation'
plot for [col=2:6] 'plot_data/hot_set_validation.tsv' using 1:col with linespoints lw 2 title columnhead(col), 'plot_data/hot_set_validation.tsv' using 1:7 with linespoints lw 3 dt 2 title 'Selected on train', 'plot_data/hot_set_validation.tsv' using 1:8 with linespoints lw 2 dt 3 lc rgb 'black' title 'Ideal'
unset multiplot
unset output
set terminal pngcairo size 1300,600
set output 'plots/hot_set.png'
set encoding utf8
set grid ytics
set yrange [0:100]
set xlabel 'Total capacity (pages)'
set ylabel 'Hit ratio (%)'
set xtics ("8" 8,"16" 16,"32" 32)
set xrange [7:33]
set key outside bottom center horizontal maxrows 3
set multiplot layout 1,2 title 'hot set'
set title 'train'
plot for [col=2:6] 'plot_data/hot_set_train.tsv' using 1:col with linespoints lw 2 title columnhead(col), 'plot_data/hot_set_train.tsv' using 1:7 with linespoints lw 3 dt 2 title 'Selected on train', 'plot_data/hot_set_train.tsv' using 1:8 with linespoints lw 2 dt 3 lc rgb 'black' title 'Ideal'
set title 'validation'
plot for [col=2:6] 'plot_data/hot_set_validation.tsv' using 1:col with linespoints lw 2 title columnhead(col), 'plot_data/hot_set_validation.tsv' using 1:7 with linespoints lw 3 dt 2 title 'Selected on train', 'plot_data/hot_set_validation.tsv' using 1:8 with linespoints lw 2 dt 3 lc rgb 'black' title 'Ideal'
unset multiplot
unset output
