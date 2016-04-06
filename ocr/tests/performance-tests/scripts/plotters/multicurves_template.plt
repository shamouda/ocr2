### n: change this parameter to equal the number of data sets to be plotted
n = 1
# t: top margin in pixels
t = 75.0
# b: key height in pixels (bottom margin)
b = 300.0
# h: height of output in pixels
h = 150.0*n + t + b

### define functions to help set top/bottom margins
top(i,n,h,t,b) = 1.0 - (t+(h-t-b)*(i-1)/n)/h
bot(i,n,h,t,b) = 1.0 - (t+(h-t-b)*i/n)/h

set term OUTPUT_IMG_FORMAT_VAR
set output 'OUTPUT_NAME_VAR'

# set key center center
# set tmargin at screen bot(n,n,h,t,b)
# set bmargin at screen 0
# plot 2

set title 'TITLE_VAR'
set ylabel 'YLABEL_VAR'
#set xtics (XTICKS_VAR)
set xlabel 'XLABEL_VAR'
# set size 1, 0.75
# set lmargin at screen 0.5
set key outside bottom center
#set tmargin at screen top(1,n,h,t,b)
# set bmargin at screen bot(1,n,h,t,b)
plot CURVES_VAR
# unset title
# unset ylabel
# unset xtics
# unset xlabel

#set key center center
#set tmargin at screen bot(n,n,h,t,b)
#set bmargin at screen 0
#set border 0

#plot CURVES_VAR
