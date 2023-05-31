#!/bin/bash

servers=(
c220g2-010629.wisc.cloudlab.us
c220g2-010603.wisc.cloudlab.us
c220g2-011129.wisc.cloudlab.us
c220g2-010632.wisc.cloudlab.us
c220g2-010815.wisc.cloudlab.us
c220g2-010811.wisc.cloudlab.us
c220g2-010631.wisc.cloudlab.us
c220g2-010804.wisc.cloudlab.us
c220g2-010802.wisc.cloudlab.us
c220g2-011124.wisc.cloudlab.us
c220g2-011131.wisc.cloudlab.us
c220g2-010604.wisc.cloudlab.us
c220g2-010627.wisc.cloudlab.us
c220g2-010630.wisc.cloudlab.us
)
	  
for server in "${servers[@]}"
do
	echo ${server}
	ssh ${server} 'mkdir ~/workspace/traces/'
	scp *.tm ${server}:~/workspace/traces/
	#ssh janechen@${server} "cat /proj/wisr-PG0/yanfang/id_rsa.pub >> ~/.ssh/authorized_keys"
	#ssh janechen@${server} "ssh-keyscan github.com >> ~/.ssh/known_hosts"
	#  ssh janechen@${server} "sh /proj/wisr-PG0/yanfang/install-gcc5.sh"
	# ssh janechen@${server} "sh /proj/wisr-PG0/yanfang/install-sw.sh"
done

