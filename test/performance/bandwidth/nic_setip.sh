start=$1
num=$2
interface=$3
declare -i i=1
while ((i<=8))
do
    let j=start+i
    echo 192.168.31.$j
    ip addr add 192.168.31.$j/24 dev $interface
    let i=i+1
done