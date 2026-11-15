cent="$1"
vol="$2"
sample="$3"
volF="$4"

root -b -l<<EOF
.x GenerateEventsOO.cxx($cent, $vol/10., $sample)
EOF

#root -b -l<<EOF
#.x GenerateEventsPP.cxx($cent, $vol/10., $sample)
#EOF

# //, $volF/100.)

