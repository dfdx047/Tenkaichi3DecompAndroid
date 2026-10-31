# lowend.sh on <gpu MHz> | off | show: the Cube as a Mali-G52 MC1 phone (1 GPU core at that clock, only the four A55 at 1.846 GHz)
D=192.168.1.183:5555; M=/sys/class/misc/mali0/device; G=/sys/class/devfreq/23140000.gpu; C=/sys/devices/system/cpu
case $1 in
on)  adb -s $D shell "su -c 'echo 1 > $M/core_mask; echo ${2}000000 > $G/min_freq; echo ${2}000000 > $G/max_freq; for i in 4 5 6 7; do echo 0 > $C/cpu\$i/online; done; echo 1846000 > $C/cpu0/cpufreq/scaling_max_freq'" ;;
off) adb -s $D shell "su -c 'echo 50005 > $M/core_mask; for i in 4 5 6 7; do echo 1 > $C/cpu\$i/online; done; echo 850000000 > $G/max_freq; echo 850000000 > $G/min_freq; echo 2184000 > $C/cpu0/cpufreq/scaling_max_freq; echo 2210000 > $C/cpu4/cpufreq/scaling_max_freq; echo 2704000 > $C/cpu7/cpufreq/scaling_max_freq'" ;;
esac
adb -s $D shell "su -c 'echo gpu: \$(cat $M/core_mask | tr \"\n\" \" \" | cut -c1-40) \$(cat $G/min_freq)-\$(cat $G/max_freq) cur \$(cat $G/cur_freq); echo cpus online: \$(cat $C/online), cpu0 max \$(cat $C/cpu0/cpufreq/scaling_max_freq)'"
