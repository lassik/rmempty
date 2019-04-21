package main

import (
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"path"
	"strconv"
)

func rmEmptyFile(fullPath string) {
	fmt.Println("rm", strconv.Quote(fullPath))
	os.Remove(fullPath)
}

func rmEmptyDir(fullPath string) {
	fmt.Println("rmdir", strconv.Quote(fullPath))
	os.Remove(fullPath)
}

func isEmptyFile(fullPath string, info os.FileInfo) bool {
	if !info.Mode().IsRegular() {
		return false
	}
	if info.Size() == 0 {
		return true
	}
	return false
}

func walkDir(dirname string) bool {
	isEmpty := true
	infos, err := ioutil.ReadDir(dirname)
	if err != nil {
		log.Fatal(err)
	}
	for _, info := range infos {
		walkEnt(path.Join(dirname, info.Name()))
	}
	if isEmpty {
		rmEmptyDir(dirname)
	}
	return isEmpty
}

func walkEnt(fullPath string) bool {
	info, err := os.Lstat(fullPath)
	if err != nil {
		log.Fatal(err)
	}
	if info.IsDir() && walkDir(fullPath) {
		return true
	}
	if isEmptyFile(fullPath, info) {
		rmEmptyFile(fullPath)
		return true
	}
	return false
}

func main() {
	flag.Parse()
	for _, fullPath := range flag.Args() {
		walkEnt(fullPath)
	}
}
