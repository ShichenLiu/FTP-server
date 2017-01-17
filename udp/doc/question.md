1. How to write a chat program(two clients chat with each other) with UDP?
    
Each client serves as both client and server, which means using two threads to recieve from and send to each other simultaneously.

2. Can we use the UDP to transfer a file? If so, how?

Of course we can. I come up with two different ideas.
    First: use UPD as TCP, they establish several reliable transportations. For each package of data, make sure the client has received the data, then moves to the next package.
    Second: like what we did in the assignment. First we send all packages in sequence with its serial number. Then we check what returned by the server. By analysing the returned serial number, we can see which blocks of data is missing. Then resend those data in series, till all the data blocks are assured.
