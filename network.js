/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/

class Network
{
    network = null;

    constructor( container )
    {
        let edges = new vis.DataSet();
        let nodes = new vis.DataSet();
        let data = { nodes: nodes
                   , edges: edges };
        //  TODO: All these options should exist in a settings node, configurable by the user.
        let options = { height: '100%'
                      , width: '100%'
                      , autoResize: true
                      , layout: { hierarchical: { direction: 'LR'
                                                // , sortMethod: 'directed' Disabled this setting, as enabling it caused all leaf nodes to become aligned.
                                                , nodeSpacing: 50
                                                , levelSeparation: 250
                                                , blockShifting: true
                                                , edgeMinimization: true
                                                , parentCentralization: true } }
                      , physics: { enabled: false
                                 , hierarchicalRepulsion: { nodeDistance: 1 } }
                      , interaction: { dragNodes: false }
                      , edges: { smooth: { type: 'continuous'
                                         , forceDirection: 'horizontal' } }
                      , nodes: { shape: 'box'
                               , widthConstraint: { minimum: 200
                                                  , maximum: 200 } } };
        this.network = new vis.Network( container
                                      , data
                                      , options );
        
        this.network.canvas.frame.onkeydown = network_onkeydown;
        this.network.on( 'deselectNode', network_on_deselect_node_event( this ) );
        this.network.on( 'selectNode', network_on_select_node_event( this ) );
    }

    add_edge( edge_id
            , from_id
            , to_id )
    {
        this.network.body.data.edges.add( { id: edge_id
                                          , from: from_id
                                          , to: to_id } );
    }

    // TODO: Ensure that from_ids.size() == to_ids.size()
    add_edges( edge_ids
             , from_ids
             , to_ids )
    {
        let data = [];
        
        for( i in from_ids )
        {
            data.push( { id: edge_ids[ i ]
                       , from: from_ids[ i ]
                       , label: to_ids[ i ] } );
        }

        this.network.body.data.edges.add( data );
    }

    center_viewport_node( node_id )
    {
        let pos = this.network.getPositions( [ node_id ] )[ node_id ];

        this.network.moveTo( { position: { x: pos.x
                                         , y: pos.y } } );
    }

    change_node_font( uuid
                    , new_face
                    , new_color )
    {
        let n = this.network.body.data.nodes.get( uuid );

        n.font = { color: new_color
                , face: new_face };

        this.network.body.data.nodes.update( n );
    }

    children_ids( node_id )
    {
        return this.network.getConnectedNodes( node_id, 'to' );
    }

    color_node_background( uuid
                         , color )
    {
        let n = this.network.body.data.nodes.get( uuid );

        n.color = { background: color
                  , highlight: { background: color } };

        this.network.body.data.nodes.update( n );
    }

    color_node_border( uuid
                     , color )
    {
        let n = this.network.body.data.nodes.get( uuid );

        n.color = { border: color
                  , background: '#f0f0f0' 
                  , highlight: { border: color
                               , background: '#f0f0f0' } };

        this.network.body.data.nodes.update( n );
    }

    // TODO: Ensure that from_ids.size() == to_ids.size()
    create_edges( edge_ids
                , from_ids
                , to_ids )
    {
        let data = [];
        
        for( i in from_ids )
        {
            data.push( { id: edge_ids[ i ]
                     , from: from_ids[ i ]
                     , to: to_ids[ i ] } );
        }

        this.network.body.data.edges.add( data );
    }

    create_node( uuid
               , title )
    {
        this.network.body.data.nodes.add( { id: uuid
                                          , label: title } );
    }

    // TODO: Ensure that uuids.size() == titles.size()
    create_nodes( uuids
                , titles )
    {
        let data = [];
        
        for( i in uuids )
        {
            data.push( { id: uuids[ i ]
                    , label: titles[ i ] } );
        }

        this.network.body.data.nodes.add( data );
    }

    destroy_network()
    {
        this.network.destroy();
    }

    edge_exists( edge_id )
    {
        return this.network.body.data.edges.get( edge_id ) !== null;
    }

    focus_network()
    {
       this.network.canvas.frame.focus();
    }

    id_exists( id )
    {
        let ids = this.network.body.data.nodes.getIds();

        return ids.indexOf( id ) != -1;
    }

    node_ids()
    {
        return this.network.body.data.nodes.getIds();
    }

    node_position( node_id )
    {
        let pos = this.network.getPositions( [ node_id ] )[ node_id ];

        return pos;
    }

    parent_id( node_id )
    {
        return this.network.getConnectedNodes( node_id, 'from' )[ 0 ];
    }

    remove_edge( edge_id )
    {
        this.network.body.data.edges.remove( edge_id );
    }

    remove_edges()
    {
        this.network.body.data.edges.clear();
    }

    remove_node( nid )
    {
        this.network.body.data.nodes.remove( { id: nid } );
        this.network.body.data.edges.forEach( function( e )
        {
            if( e.from == nid
             || e.to == nid )
            {
                this.network.body.data.edges.remove( e.id );
            }
        } );
    }

    remove_nodes()
    {
        this.remove_edges();

        this.network.body.data.nodes.clear();
    }

    select_node( node_id )
    {
        this.network.setSelection( { nodes: [ node_id ]
                                   , edges: [] } );
    }
    
    selected_node_id()
    {
        let ns = this.network.getSelectedNodes();

        if( ns.length != 1 )
        {
            return null;
        }

        return ns[ 0 ];
    }

    title_of( id )
    {
        return this.network.body.data.nodes.get( id ).label;
    }

    update_title( id
                , title )
    {
        this.network.body.data.nodes.update( { id: id
                                             , label: title } );
    }
}

function new_network( container )
{
    return new Network( document.getElementById( container ) );
}

let network_on_deselect_node_event = function( network ) 
{
    return function( e )
    {
        if( e.nodes.length == 0 ) // New node wasn't selected.
        {
            // So, reselect the former.
            const prev_sel = e.previousSelection.nodes[ 0 ];
            // Note: double "select node" is non-straightforward workaround. Here's what I understand to be going on:
            // - kmap.select_node() assumes that the network always has a node selected, so if it's called without
            //   visjs having a node selected, it will err - even though it proceeds to call select_node() itslef.
            network.select_node( prev_sel );
            kmap.select_node( kmap.uuid_from_string( prev_sel ).value() ); // Assumes 'prev_sel' valid UUID.
        }
    };
};

let network_on_select_node_event = function( network ) 
{
    return function( e )
    {
        if( e.nodes.length != 0 )
        {
            const node = e.nodes[ 0 ];
            // Note: double "select node" is non-straightforward workaround. Here's what I understand to be going on:
            // - kmap.select_node() assumes that the network always has a node selected, so if it's called without
            //   visjs having a node selected, it will err - even though it proceeds to call select_node() itslef.
            network.select_node( node );
            kmap.select_node( kmap.uuid_from_string( node ).value() ); // Assumes 'node' valid UUID.
        }
    };
};

function network_onkeydown( e )
{
    let key = e.keyCode ? e.keyCode : e.which;
    let is_ctrl = !!e.ctrlKey;
    let is_shift = !!e.shiftKey;

    switch ( key )
    {
    case 69: // e
    {
        kmap.edit_body();
        e.preventDefault();
        break;
    }
    case 72: // h
    case 37: // left-arrow
    {
        kmap.travel_left();
        e.preventDefault();
        break;
    }
    case 74: // j
    case 40: // down-arrow
    {
        kmap.travel_down();
        e.preventDefault();
        break;
    }
    case 75: // k
    case 38: // up-arrow
    {
        kmap.travel_up();
        e.preventDefault();
        break;
    }
    case 76: // l
    case 39: // right-arrow
    {
        kmap.travel_right();
        e.preventDefault();
        break;
    }
    case 73: // i
    {
        if( is_ctrl )
        {
            console.log( 'ctrl+i' );
            kmap.jump_in();
        }
        break;
    }
    case 79: // o
    {
        if( is_ctrl )
        {
            console.log( 'ctrl+o' );
            kmap.jump_out();
        }
        break;
    }
    case 86: // v
    {
        kmap.view_body();
        e.preventDefault();
        break;
    }
    case 71: // g
    {
        if( is_shift ) // G
        {
            kmap.travel_bottom();
        }
    }
    }
}
