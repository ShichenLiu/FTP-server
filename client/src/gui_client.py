import subprocess
from time import sleep
from subprocess import PIPE, STDOUT
import os
from Tkinter import *
from tkFileDialog import askopenfilename, askdirectory
from tkSimpleDialog import askstring
from ScrolledText import ScrolledText
import ttk
import sys
reload(sys)
sys.setdefaultencoding('utf-8')

class GUI:
    def __init__(self):
        self.makefile()
        self.getDest()
        if not self.valid:
            return
        self.login()
        if self.valid:
            print "Successful login"
            self.startServer()
        else:
            print "Cancel by user"
        return

    def makefile(self):
        proc = subprocess.Popen('make', stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        while True:
            stdout = proc.stdout.readline()
            stderr = proc.stderr.readline()
            if not (stdout and stderr):
                break
        return

    def getDest(self):
        destDia = Tk()
        destDia.geometry('480x50+300+300')
        host = StringVar()
        host.set('127.0.0.1')
        port = StringVar()
        port.set('21')
        hostEntry = Entry(destDia, textvariable=host, width=20)
        portEntry = Entry(destDia, textvariable=port, width=10)
        hostEntry.grid(column=0, row=0, padx=15, pady=10)
        portEntry.grid(column=1, row=0)
        def confirm():
            self.port = port.get()
            self.host = host.get()
            self.valid = True
            destDia.destroy()
            return
        def cancel():
            self.valid = False
            destDia.destroy()
            return
        confirmButton = Button(destDia, command=confirm, text="Confirm")
        confirmButton.grid(column=2, row=0)
        cancelButton = Button(destDia, command=cancel, text="Cancel")
        cancelButton.grid(column=3, row=0)
        destDia.mainloop()
        return
    
    def login(self):
        def validInfo(preload, user, word):
            self.buff = open("client.buff", "w+")
            self.getter = open("client.buff", "r")
            wordcont = word.get()
            if wordcont == "":
                wordcont = "\'\'"
            self.client = subprocess.Popen("./client -gui -user "+user.get()+" -pass "+wordcont+" -port "+self.port+" -host "+self.host, stdin=PIPE, stdout=self.buff, stderr=STDOUT, universal_newlines=True, shell=True)
            sleep(.5)
            self.buff.seek(0, 0)
            self.initInfo = self.buff.readlines()
            print self.initInfo
            if self.initInfo[-1][:4] != "230 ":
                self.close()
                self.valid = False
                return
            self.buff.seek(0, 0)
            self.buff.truncate()
            self.valid = True
            preload.destroy()
            return
        def cancelInfo():
            self.valid = False
            preload.destroy()
            return
        preload = Tk()
        preload.geometry('520x80+300+300')
        username = StringVar()
        username.set('anonymous')
        password = StringVar()
        password.set('anonymous@mails.com')
        usernameEntry = Entry(preload, textvariable=username, width=40)
        passwordEntry = Entry(preload, textvariable=password, width=40)
        confirmButton = Button(preload, text='login', command=lambda: validInfo(preload, username, password))
        cancelButton = Button(preload, text='cancel', command=cancelInfo)
        usernameEntry.grid(column=0, row=0, padx=15, pady=10)
        passwordEntry.grid(column=0, row=1, padx=15)
        confirmButton.grid(column=1, row=0)
        cancelButton.grid(column=1, row=1)
        preload.mainloop()
        return

    def logInfo(self, tag, res):
        if not res:
            return
        if type(res) == str:
            res = [res]
        for line in res:
            self.log.insert(END, line.strip()+'\n')
        self.log.tag_add(tag, "%s.%s"%(self.col,0), "%s.%s"%(self.col+len(res),len(res[-1])))
        self.col += len(res)

    def logSucc(self, res):
        return self.logInfo("succ", res)

    def logFail(self, res):
        return self.logInfo("fail", res)

    def logWait(self, res):
        return self.logInfo("wait", res)

    def logNorm(self, res):
        return self.logInfo("norm", res)
    
    def getCmdResponse(self, cmd):
        self.client.stdin.write(cmd + '\n')
        cnt = 1
        if cmd[:4] in ('LIST', 'RETR', 'STOR'):
            cnt += 1
        while True:
            self.getter.seek(0, 0)
            buf = self.getter.readlines()
            if len(filter(lambda x: len(x) > 3 and x[:3].isdigit() and x[3] == ' ', buf)) == cnt:
                self.getter.seek(0, 0)
                break
        self.buff.seek(0, 0)
        self.logNorm(['Command>>> '+cmd])
        res = self.buff.readlines()
        self.buff.seek(0, 0)
        self.buff.truncate()
        code = int(filter(lambda x: len(x) > 3 and x[:3].isdigit() and x[3] == ' ', buf)[-1][:3])
        if 100 <= code < 300:
            if cmd == 'LIST':
                self.logSucc([res[1], res[-1]])
            else:
                self.logSucc(res)
        if 300 <= code < 400:
            self.logWait(res)
        if 400 <= code < 500:
            self.logFail(res)
        self.log.see(END)
        if cmd.startswith('LIST'):
            return res[2:-2]
        if cmd.startswith('SYST'):
            return res[-1]

    def startServer(self):
        window = Tk()
        window.geometry('800x580+200+100')
        self.log = ScrolledText(window, width=108, height=8)
        self.col = 1
        title = Label(window, text="Lustralisk client connected to "+self.getCmdResponse('SYST')[4:-2])
        title.grid(column=0, row=0, columnspan=12)
        tree = ttk.Treeview(window, selectmode="extended",columns=("path", "type", "A","B"), displaycolumns=("A", "B"), height="18")
        tree.heading("#0", text="Name")
        tree.column("#0", minwidth=200, width=400, stretch=True)
        tree.heading("A", text="Modified Date")
        tree.column("A", minwidth=200, width=180, stretch=True) 
        tree.heading("B", text="Size")
        tree.column("B", minwidth=100, width=200, stretch=True)
        tree.grid(column=0, row=1, columnspan=12, padx=8)
        self.log.grid(column=0, row=2, columnspan=12, pady=10)
        self.log.tag_config("succ", foreground="green")
        self.log.tag_config("wait", foreground="orange")
        self.log.tag_config("fail", foreground="red")
        self.log.tag_config("norm", foreground="blue")
        parent = tree.insert('', END, text='/', values=['/', 'd', '', ''], open=True)
        def quitWindow():
            self.close()
            window.destroy()
            return
        def populateNode(path, parent):
            self.getCmdResponse('CWD '+path)
            self.getCmdResponse('PASV')
            initFiles = self.getCmdResponse('LIST')
            if len(initFiles) == 0:
                tree.insert(parent, END, text='empty folder')
                return
            for f in initFiles:
                if f.startswith('d'):
                    try:
                        d = tree.insert(parent, END, text=f.split()[-1]+'/', values=[path + f.split()[-1] + '/', 'd', '%4s%3s%6s' % (f.split()[5], int(f.split()[6]), f.split()[7]), ''])
                        tree.insert(d, END, text='dummy')
                    except:
                        pass
                else:
                    try:
                        tree.insert(parent, END, text=f.split()[-1], values=[path + f.split()[-1], 'f', '%4s%3s%6s' % (f.split()[5], int(f.split()[6]), f.split()[7]), f.split()[4]])
                    except:
                        pass
            return
        def updateTree(event):
            nodeId = tree.focus()
            if tree.parent(nodeId) and tree.set(nodeId, 'type') == 'd':
                topChild = tree.get_children(nodeId)[0]
                if tree.item(topChild, option='text') == 'dummy':
                    tree.delete(topChild)
                    populateNode(tree.set(nodeId, 'path'), nodeId)
            return
        def dfsDelete(parent):
            for child in tree.get_children(parent):
                if tree.set(child, 'type') == 'd':
                    dfsDelete(child)
                    tree.delete(child)
                else:
                    tree.delete(child)
        def upload():
            filename = askopenfilename()
            if filename == '':
                self.logFail('Error<<< No filename input, aborted.')
                return
            self.getCmdResponse('PASV')
            if tree.focus():
                if tree.set(tree.focus(), 'type') == 'd':
                    uploadPath = tree.set(tree.focus(), 'path')
                else:
                    uploadPath = tree.set(tree.parent(tree.focus()), 'path')
            else:
                self.logWait('Warning<<< No path selected, using root as default.')
                uploadPath = '/'
            self.getCmdResponse('CWD '+uploadPath)
            self.getCmdResponse('STOR '+filename)
            refresh()
            return
        def download():
            if not tree.focus():
                self.logFail('Error<<< No file selected, aborted.')
                return
            if tree.set(tree.focus(), 'type') == 'f':
                self.getCmdResponse('PASV')
                self.getCmdResponse('CWD '+tree.set(tree.parent(tree.focus()), 'path'))
                self.getCmdResponse('RETR '+tree.set(tree.focus(), 'path').split('/')[-1])
                dirpath = askdirectory()
                if not dirpath == '':
                    os.rename(tree.set(tree.focus(), 'path').split('/')[-1], dirpath+'/'+tree.set(tree.focus(), 'path').split('/')[-1])
            return
        def remove():
            if not tree.focus():
                self.logFail('Error<<< No file selected, aborted.')
                return
            if tree.set(tree.focus(), 'type') == 'f':
                self.getCmdResponse('DELE '+tree.set(tree.focus(), 'path'))
            else:
                self.getCmdResponse('RMD '+tree.set(tree.focus(), 'path'))
            refresh()
            return
        def create():
            filename = askstring("title", "folder name: ")
            if filename == '':
                self.logFail('Error<<< No filename input, aborted.')
                return
            if not tree.focus():
                self.logFail('Error<<< No file selected, aborted.')
                return
            if tree.focus():
                if tree.set(tree.focus(), 'type') == 'd':
                    uploadPath = tree.set(tree.focus(), 'path')
                else:
                    uploadPath = tree.set(tree.parent(tree.focus()), 'path')
            else:
                self.logWait('Warning<<< No path selected, using root as default.')
                uploadPath = '/'
            self.getCmdResponse('CWD '+uploadPath)
            self.getCmdResponse('MKD '+filename)
            refresh()
            return
        def rename():
            if not tree.focus():
                self.logFail('Error<<< No file selected, aborted.')
                return
            name = askstring("title", "rename to: ")
            if name == '':
                self.logFail('Error<<< No filename input, aborted.')
                return
            path = tree.set(tree.focus(), 'path')
            if path[-1] == '/' and len(path) > 1:
                path = path[:-1]
            self.getCmdResponse('RNFR '+path)
            path = '/'.join(path.split('/')[:-1])+'/'+name
            self.getCmdResponse('RNTO '+path)
            refresh()
            return
        def refresh():
            dfsDelete(parent)
            populateNode('/', parent)
        tree.bind('<<TreeviewOpen>>', updateTree)
        populateNode('/', parent)
        Button(window, text="refresh", command=refresh).grid(column=0, row=3)
        Button(window, text="create", command=create).grid(column=1, row=3)
        Button(window, text="upload", command=upload).grid(column=2, row=3)
        Button(window, text="download", command=download).grid(column=3, row=3)
        Button(window, text="remove", command=remove).grid(column=5, row=3)
        Button(window, text="rename", command=rename).grid(column=6, row=3)
        Button(window, text="quit", command=quitWindow).grid(column=11, row=3)
        window.mainloop()
        return

    def close(self):
        self.client.stdin.write('QUIT\r\n')
        self.client.stdin.flush()
        self.client.stdin.close()
        self.client.kill()
        self.getter.close()
        self.buff.close()
        os.remove('client.buff')
        return
    
gui = GUI()
