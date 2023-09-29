'use strict'

const {User, Edge} = require('m2m')  

// Create a user object to authenticate your edge application
let user = new User()

let edge = new Edge()

let n = 0

async function main(){

    // authenticate the edge application
    await user.connect() 

    /**
     * Create an edge client
     */
    let ec = new edge.client({ port:5300, ip:'127.0.0.1', secure:false, restart:false }) 

    let interval = setInterval(async () => {
        // read the data from the edge connector
        let data = await ec.read('random-data')

        // stop the data collection after 5 samples
        if(n === 5){
            console.log('no. of sample data', n)
            return clearInterval(interval)
        }     

        try{
            let jd = JSON.parse(data)
            console.log('ec read random-data value:', jd.value)
            n++
        }
        catch (e){
            console.log('json parse error: ', data1.toString())
        }

    }, 5000)
}

main()
