INCLUDE = include
P7DIR = p7
EV1DIR = ev1
S1DIR = s1

.PHONY: p7 s1

p7:
	$(MAKE) -C $(P7DIR)

p7clean:
	$(MAKE) -C $(P7DIR) clean

s1:
	$(MAKE) -C $(S1DIR)

s1clean:
	$(MAKE) -C $(S1DIR) clean
