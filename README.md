dumbwaiter
==========
A RESTful message passing abstraction library.

Dumbwaiter originated as an attempt to make it easy to use POSIX message queues in Linux. The scope of the project has begun to expand to also be an abstraction on top of *all* available underlying OS message passing mechanisms, `cause for the most part I don't care how the message gets there, I just want it to get there.

What I mean by "there" is a bit abstract now, it could be another process, another thread, or even the same process and same thread. It all depends on how you use it. Whatever "there" is simply needs to register itself as an "agent" (also could have chosen the term "actor"), giving itself a name in the process, and in return it gets a "key" which is merely an integer that is *supposed* to be unique across the field. When an agent wants to send something it registers that something as a "resource," giving it a name in the process. It is then free to send this resource to another agent, perhaps first getting the receiving agent's key using its name if it does not have it already. To receive the message, the receiving agent must either be waiting on a blocking call to _receive()_ or periodically calling the non-blocking _receive()_ 

I'm also in the process of starting to use paths instead of names, so right now all "names" must begin with a forward slash (/) AND I'm also working on adapting Dumbwaiter to easily interface with Ditz, my stupid DBMS.
