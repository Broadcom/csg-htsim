#!/bin/bash

servers=(
c220g2-010805.wisc.cloudlab.us
c220g2-011322.wisc.cloudlab.us
c220g2-030831.wisc.cloudlab.us
c220g2-030832.wisc.cloudlab.us
c220g2-030631.wisc.cloudlab.us
c220g2-011107.wisc.cloudlab.us
c220g2-011116.wisc.cloudlab.us
)
	  
for server in "${servers[@]}"
do
	echo ${server}
	#ssh yfle0707@${server} 'sudo chown -R yfle0707 /mydata; mkdir -p /mydata/workspace/traces/'
	scp *.tm yfle0707@${server}:/mydata/workspace/traces/
	#ssh yfle0707@${server} "sudo loginctl enable-linger yfle0707"
	#scp config yfle0707@${server}:~/.ssh/
	#ssh yfle0707@${server} "ssh -T git@github.com"
	#ssh janechen@${server} "cat /proj/wisr-PG0/yanfang/id_rsa.pub >> ~/.ssh/authorized_keys"
	#ssh janechen@${server} "ssh-keyscan github.com >> ~/.ssh/known_hosts"
	#  ssh janechen@${server} "sh /proj/wisr-PG0/yanfang/install-gcc5.sh"
	# ssh janechen@${server} "sh /proj/wisr-PG0/yanfang/install-sw.sh"
done

