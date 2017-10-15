package main

import (
	"flag"
	//"fmt"
	"github.com/larspensjo/config"
	"github.com/tarm/serial"
	"html/template"
	"io"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"strconv"
	"strings"
	"time"
)

var (
	conFile = flag.String("configfile", "config.ini", "config file")
	TOPIC   = make(map[string]string)
	con     Config
)

type Config struct {
	CommID       string //串口端口号
	BaudRate     int
	Port         string //监听端口
	IP           string
	WanCmdUpdate int //外网更新指令时间间隔
}

//读取配置文件
func ReadConfig() Config {
	var c Config
	file, _ := os.Getwd() //获取当前路径
	cfg, err := config.ReadDefault(file + "/" + *conFile)
	if err != nil {
		log.Fatal("读取配置文件错误: " + err.Error())
	}
	c.CommID, _ = cfg.String("COM", "COMID") //获取配置文件中的配置项
	rate, _ := cfg.String("COM", "BaudRate")
	c.BaudRate, _ = strconv.Atoi(rate)
	c.Port, _ = cfg.String("SYS", "Port") //服务监听端口
	c.IP, _ = cfg.String("SYS", "IP")
	c.WanCmdUpdate, _ = cfg.Int("SYS", "WanCmdUpdate")
	return c
}

func OpenCOM() (io.ReadWriteCloser, error) {
	c := &serial.Config{Name: con.CommID, Baud: con.BaudRate} //设置串口
	s, err := serial.OpenPort(c)                              //打开串口

	if err != nil {
		log.Fatal(err)
	}
	return s, err
}

//向串口写入
func SerialWrite(cmd string) (string, error) {
	ser, err := OpenCOM() //初始化串口
	_, err = ser.Write([]byte(cmd + ";\n"))
	if err != nil {
		return "", err
	}
	time.Sleep(time.Second / 2)
	buf := make([]byte, 1024)
	n, err := ser.Read(buf)
	if err != nil {
		return "", err
	}
	defer ser.Close()
	return string(buf[:n]), err
}

func Index(w http.ResponseWriter, r *http.Request) {
	var cmd string
	r.ParseForm()
	//fmt.Println(r.Form)
	if len(r.Form["set"]) > 0 { //串口命令
		cmd = strings.ToUpper(r.Form["set"][0])
		log.Println("本地指令: " + cmd)
		cmd, err := SerialWrite(cmd)
		if err == nil {
			io.WriteString(w, cmd)
		} else {
			io.WriteString(w, err.Error())
		}
	} else { //若URL无参数,显示首页内容
		t, err := template.ParseFiles("index.html")
		if err != nil {
			log.Println(err)
		}
		t.Execute(w, nil)
	}
}

//获取外网存放的指令
func ReadWanCommand() {
	for {
		time.Sleep(time.Duration(con.WanCmdUpdate) * time.Second)
		resp, err := http.Get("http://car.99anhao.net/?get")
		if err != nil {
			log.Println(err.Error())
		} else {
			defer resp.Body.Close()
			body, err := ioutil.ReadAll(resp.Body)
			if err != nil {
				log.Println(err.Error())
			} else {
				cmd := string(body)
				if cmd != "" {
					log.Println("网络指令: " + cmd)
					SerialWrite(cmd)
				}
			}
		}
	}
}

func main() {
	con = ReadConfig() //读取配置文件
	go ReadWanCommand()

	http.HandleFunc("/", Index) //首页
	log.Printf("控制服务器启动，%s:%s", con.IP, con.Port)
	err := http.ListenAndServe(con.IP+":"+con.Port, nil)
	if err != nil {
		log.Fatal(err)
	}
	/*
		command, err := cfg.String("COM", "COMMAND")
		// 写入串口命令
		log.Printf("写入的指令 %s", command)
		n, err := s.Write([]byte(command))

		if err != nil {
			log.Fatal(err)
		}

		buf := make([]byte, 128)
		n, err = s.Read(buf)
		log.Printf("读取信息 %s", buf[:n])
		if err != nil {
			log.Fatal(err)
		}
		log.Printf("%q", buf[:n])
	*/
}
